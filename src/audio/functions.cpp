
// Slew by 0.01 gain per output frame: a full-scale transition takes 100 frames at every source rate.
constexpr double glRampStep = 0.01;
constexpr int glRampFrames = 100;

#include <type_traits>  // For SFINAE and template metaprogramming
#include "mixer_dispatch.h"
#include "audio_batch.h"

static void filter_float_mono(extAudio *, float *, int);
static void filter_float_stereo(extAudio *, float *, int);
static void mix_channel(extAudio *, AudioChannel &, int, APTR);
static ERR mix_data(extAudio *, int, APTR);
static ERR process_commands(extAudio *, SAMPLE);

//********************************************************************************************************************

inline bool adjust_volume_ramp(double &current, double target, double ramp_speed) {
   if (current < target) {
      current += ramp_speed;
      if (current >= target) {
         current = target;
         return false;
      }
      return true;
   }
   else if (current > target) {
      current -= ramp_speed;
      if (current <= target) {
         current = target;
         return false;
      }
      return true;
   }
   return false;
}

//********************************************************************************************************************
// Constexpr clamping functions for compile-time optimization

template<typename T>
constexpr T clamp_sample(T value, T min_val, T max_val) noexcept {
   return (value < min_val) ? min_val : ((value > max_val) ? max_val : value);
}

//********************************************************************************************************************
// SIMD-aware conversion with template specialization and loop unrolling

template<typename OutputType, typename InputType = float, std::size_t UnrollFactor = 4>
void convert_samples(const InputType *Input, int Count, OutputType *Output,
   AudioDitherState *Dither = nullptr, unsigned Channels = 1) {
   auto input = Input;
   auto output = Output;
   const int count = Count;
   if constexpr (std::is_same_v<OutputType, uint8_t> or std::is_same_v<OutputType, int16_t>) {
      if (Dither) {
         constexpr double scale = std::is_same_v<OutputType, uint8_t> ? 128.0 : 32768.0;
         constexpr double offset = std::is_same_v<OutputType, uint8_t> ? 128.0 : 0.0;
         for (int i = 0; i < count; ++i) {
            const double noise = Dither->next(Channels);
            const double sample = double(input[i]);
            // Preserve exact silence and saturate at both representable endpoints, including overloads.
            double value;
            if (std::isnan(sample) or sample IS 0) value = 0;
            else if (sample <= -1) value = -scale;
            else if (sample >= (scale - 1) / scale) value = scale - 1;
            else value = std::clamp(std::floor(sample * scale + noise + 0.5), -scale, scale - 1);
            output[i] = OutputType(value + offset);
         }
         return;
      }
   }
   static_assert(std::is_arithmetic_v<OutputType>, "Output type must be arithmetic");
   static_assert(UnrollFactor > 0 and UnrollFactor <= 8, "Unroll factor must be 1-8");

   const InputType* end = input + count;
   const InputType* unroll_end = input + (count - count % UnrollFactor);

   if constexpr (std::is_same_v<OutputType, uint8_t>) {
      // 8-bit conversion with loop unrolling

      while (input < unroll_end) {
         for (std::size_t i = 0; i < UnrollFactor; ++i) {
            const auto sample = clamp_sample(InputType(input[i]), InputType(-1), InputType(127.0 / 128.0));
            output[i] = uint8_t((sample + InputType(1)) * InputType(128));
         }
         input += UnrollFactor;
         output += UnrollFactor;
      }

      // Handle remaining samples

      while (input < end) {
         const auto sample = clamp_sample(InputType(*input), InputType(-1), InputType(127.0 / 128.0));
         *output = uint8_t((sample + InputType(1)) * InputType(128));
         ++input; ++output;
      }
   }
   else if constexpr (std::is_same_v<OutputType, int16_t>) {
      // 16-bit conversion with loop unrolling
      while (input < unroll_end) {
         for (std::size_t i = 0; i < UnrollFactor; ++i) {
            const auto sample = clamp_sample(InputType(input[i]), InputType(-1), InputType(32767.0 / 32768.0));
            output[i] = int16_t(sample * InputType(32768));
         }
         input += UnrollFactor;
         output += UnrollFactor;
      }

      while (input < end) {
         const auto sample = clamp_sample(InputType(*input), InputType(-1), InputType(32767.0 / 32768.0));
         *output = int16_t(sample * InputType(32768));
         ++input; ++output;
      }
   }
   else if constexpr (std::is_same_v<OutputType, float>) {
      // Float conversion with vectorization hints
      while (input < unroll_end) {
         for (std::size_t i = 0; i < UnrollFactor; ++i) {
            output[i] = clamp_sample(float(input[i]), -1.0f, 1.0f);
         }
         input += UnrollFactor;
         output += UnrollFactor;
      }

      while (input < end) {
         *output = clamp_sample(float(*input), -1.0f, 1.0f);
         ++input; ++output;
      }
   }
   else {
      static_assert(std::is_same_v<OutputType, void>, "Unsupported output type");
   }
}

//********************************************************************************************************************

static void audio_stopped_event(extAudio &Audio, int SampleHandle)
{
   FUNCTION callback;
   {
      std::lock_guard lock(Audio.MixerMutex);
      if (size_t(SampleHandle) >= Audio.Samples.size()) return;
      auto &sample = Audio.Samples[SampleHandle];
      if (!sample.OnStop.defined()) return;
      if (sample.OnStop.stale()) {
         release_audio_callback(sample.OnStop);
         return;
      }
      callback = sample.OnStop;
      callback.pin();
   }

   if (callback.isC()) {
      kt::SwitchContext context(callback.Context);
      auto routine = (void (*)(extAudio *, int, APTR))callback.Routine;
      routine(&Audio, SampleHandle, callback.Meta);
   }
   else if (callback.isScript()) {
      sc::Call(callback, std::to_array<ScriptArg>({ { "Audio", &Audio, FD_OBJECTPTR }, { "Handle", SampleHandle } }));
   }
   callback.unpin();
}

//********************************************************************************************************************
// The callback must return the number of bytes written to the buffer.

static BYTELEN fill_stream_buffer(int Handle, AudioSample &Sample, int64_t Offset)
{
   if (Sample.Callback.stale()) return BYTELEN(0);

   if (Sample.Callback.isC()) {
      kt::SwitchContext context(Sample.Callback.Context);
      auto routine = (int (*)(int, int64_t, uint8_t *, int, APTR))Sample.Callback.Routine;
      return BYTELEN(routine(Handle, Offset, Sample.Data.data(),
         int(int64_t(Sample.SampleLength) * Sample.FrameBytes), Sample.Callback.Meta));
   }
   else if (Sample.Callback.isScript()) {
      std::span span(Sample.Data.data(), size_t(Sample.SampleLength) * Sample.FrameBytes);
      const auto args = std::to_array<ScriptArg>({
         { "Handle", Handle },
         { "Offset", Offset },
         { "Buffer", &span, FDF_SPAN|FD_BYTE }
      });

      ERR result;
      auto script = (objScript *)Sample.Callback.Context;
      script->Results.clear();
      if (sc::Call(Sample.Callback, args, result) IS ERR::Okay and result IS ERR::Okay) {
         std::span<std::string> results;
         if (script->getResults(results) IS ERR::Okay and !results.empty()) {
            return BYTELEN(std::clamp<int64_t>(strtoll(results[0].c_str(), nullptr, 10), 0, span.size()));
         }
      }
      return BYTELEN(0);
   }

   return BYTELEN(0);
}

//********************************************************************************************************************

[[maybe_unused]] static ERR get_mix_amount(extAudio *Self, SAMPLE *MixLeft)
{
   auto ml = SAMPLE(0x7fffffff);
   for (int i=1; i < (int)Self->Sets.size(); i++) {
      if ((Self->Sets[i].MixLeft > 0) and (Self->Sets[i].MixLeft < ml)) {
         ml = Self->Sets[i].MixLeft;
      }
   }

   *MixLeft = ml;
   return ERR::Okay;
}

//********************************************************************************************************************
// Linear gain with a linear balance pan law.  The side being panned towards keeps the full gain and the opposite side
// is attenuated linearly, reaching silence at -1.0 or 1.0.  The centre therefore plays both sides at full gain
// (not constant power).  Mono output ignores pan.  Inputs are clamped to 0 - 1.0 and -1.0 - 1.0 respectively;
// callers must reject non-finite values.

struct ChannelGains { double Left, Right; };

static ChannelGains channel_gains(double Volume, double Pan, bool Stereo)
{
   const double volume = std::clamp(Volume, 0.0, 1.0);
   const double pan = std::clamp(Pan, -1.0, 1.0);
   if (!Stereo) return { volume, volume };
   return { volume * (1.0 - std::max(pan, 0.0)), volume * (1.0 + std::min(pan, 0.0)) };
}

//********************************************************************************************************************
// Defines the L/RVolume and Ramping values for an AudioChannel.  These values are derived from the Volume and Pan.

static ERR set_channel_volume(extAudio *Self, AudioChannel *Channel)
{
   AudioLog log(__FUNCTION__);

   if ((!Self) or (!Channel)) return log.warning(ERR::NullArgs);

   Channel->Volume = std::clamp(Channel->Volume, 0.0, 1.0);
   Channel->Pan = std::clamp(Channel->Pan, -1.0, 1.0);

   // Convert the volume into left/right volume parameters

   double leftvol = 0, rightvol = 0;
   if ((Channel->Flags & CHF::MUTE) IS CHF::NIL) {
      auto gains = channel_gains(Channel->Volume, Channel->Pan, Self->Stereo);
      leftvol  = gains.Left;
      rightvol = gains.Right;
   }

   // Start volume ramping if necessary

   Channel->Flags &= ~CHF::VOL_RAMP;
   if ((Self->Flags & ADF::VOL_RAMPING) != ADF::NIL) {
      if ((Channel->LVolume != leftvol) or (Channel->LVolumeTarget != leftvol)) {
         Channel->Flags |= CHF::VOL_RAMP;
         Channel->LVolumeTarget = leftvol;
      }

      if ((Channel->RVolume != rightvol) or (Channel->RVolumeTarget != rightvol)) {
         Channel->Flags |= CHF::VOL_RAMP;
         Channel->RVolumeTarget = rightvol;
      }
   }
   else {
      Channel->LVolume       = leftvol;
      Channel->LVolumeTarget = leftvol;
      Channel->RVolume       = rightvol;
      Channel->RVolumeTarget = rightvol;
   }

   return ERR::Okay;
}

//********************************************************************************************************************
// Execute one complete batch per channel set at its next mixer update boundary.

[[maybe_unused]] static ERR process_commands(extAudio *Self, SAMPLE Elements)
{
   for (int index=1; index < (int)Self->Sets.size(); index++) {
      advance_audio_tick(Self->Sets[index], Elements, Self->OutputRate, [Self](const AudioCommand &Command) {
         execute_audio_command(Self, Command);
      });
   }

   return ERR::Okay;
}

//********************************************************************************************************************

// Template-based conversion functions (legacy wrappers for compatibility)

static void convert_float8(extAudio *Self, float *Buffer, int TotalSamples, uint8_t *Dest) {
   convert_samples<uint8_t>(Buffer, TotalSamples, Dest,
      Self->WindowDither ? &Self->DitherState : nullptr, Self->Stereo ? 2 : 1);
}

static void convert_float16(extAudio *Self, float *Buffer, int TotalSamples, int16_t *Dest) {
   convert_samples<int16_t>(Buffer, TotalSamples, Dest,
      Self->WindowDither ? &Self->DitherState : nullptr, Self->Stereo ? 2 : 1);
}

static void convert_float(float *buf, int TotalSamples, float *dest) {
   convert_samples<float>(buf, TotalSamples, dest);
}

//********************************************************************************************************************
// Return a maximum of 32k-1 samples to prevent overflow problems

static int samples_until_end(extAudio *Self, AudioChannel &Channel)
{
   auto &sample = Self->Samples[Channel.SampleHandle];
   const auto start = Channel.LoopIndex IS 2 ? sample.Loop2Start : sample.Loop1Start;
   const auto end = Channel.LoopIndex IS 2 ? sample.Loop2End : sample.Loop1End;
   const auto type = Channel.LoopIndex IS 2 ? sample.Loop2Type : sample.Loop1Type;
   const bool backward = (Channel.Flags & CHF::BACKWARD) != CHF::NIL;
   // Reverse includes the frame at the lower endpoint. Subtracting the fraction here used to stall at it.
   const int64_t distance = backward ?
      (Channel.Position - (type IS LTYPE::NIL ? 0 : int64_t(start) -
         (type IS LTYPE::BIDIRECTIONAL ? 1 : 0))) * 65536 + Channel.PositionLow + 1 :
      ((type IS LTYPE::NIL ? int64_t(sample.SampleLength) : int64_t(end)) - Channel.Position) * 65536 -
         Channel.PositionLow;
   return int(std::clamp(distance, int64_t(0), int64_t(0x7fff0000)));
}

//********************************************************************************************************************

static bool amiga_change(extAudio *Self, AudioChannel &Channel)
{
   // A sample end or sample loop end has been reached, the sample has been changed, and both old and new samples use
   // Amiga compatible looping - handle Amiga Loop Emulation sample change

   if (Channel.SampleHandle > 1) Channel.SampleHandle--;
   auto &sample = Self->Samples[Channel.SampleHandle];
   Channel.Flags &= ~CHF::CHANGED;

   if (sample.LoopMode IS LOOP::AMIGA) {
      // Looping - start playback from loop beginning
      Channel.Position    = sample.Loop1Start;
      Channel.PositionLow = 0;
      return false;
   }

   // Not looping - finish the sample
   Self->finish(Channel, true);
   return true;
}

//********************************************************************************************************************

// Duplicated endpoints occupy a 2*length logical period. Reverse fractional positions can lie just below
// LoopStart, where the common neighbour resolver reflects their support. Modulo also handles multi-turn steps.
static void fold_bidirectional(AudioChannel &Channel, int64_t Start, int64_t End)
{
   const bool backward = (Channel.Flags & CHF::BACKWARD) != CHF::NIL;
   const int64_t length = (End - Start) * 65536;
   const int64_t position = (Channel.Position - Start) * 65536 + Channel.PositionLow;
   int64_t phase = (backward ? 2 * length - 65536 - position : position) % (2 * length);
   if (phase < 0) phase += 2 * length;
   const bool reverse = phase >= length;
   const int64_t folded = reverse ? 2 * length - 65536 - phase : phase;
   Channel.Position = Start + (folded >> 16);
   Channel.PositionLow = folded & 0xffff;
   if (reverse) Channel.Flags |= CHF::BACKWARD;
   else Channel.Flags &= ~CHF::BACKWARD;
   Channel.LoopHistory = true;
}

static bool handle_sample_end(extAudio *Self, AudioChannel &Channel)
{
   AudioLog log(__FUNCTION__);
   SAMPLE lp_start, lp_end;
   LTYPE lp_type;

   auto &sample = Self->Samples[Channel.SampleHandle];

   if (Channel.LoopIndex IS 2) {
      lp_start = sample.Loop2Start;
      lp_end   = sample.Loop2End;
      lp_type  = sample.Loop2Type;
   }
   else {
      lp_start = sample.Loop1Start;
      lp_end   = sample.Loop1End;
      lp_type  = sample.Loop1Type;
   }

   if (lp_type IS LTYPE::NIL) { // No loop - did we reach sample end?
      if (Channel.Position >= sample.SampleLength or Channel.Position < 0) {
         auto &prev_sample = Self->Samples[Channel.SampleHandle-1];
         if (((Channel.Flags & CHF::CHANGED) != CHF::NIL) and
             ((sample.LoopMode IS LOOP::AMIGA) or (sample.LoopMode IS LOOP::AMIGA_NONE)) and
             ((prev_sample.LoopMode IS LOOP::AMIGA) or (prev_sample.LoopMode IS LOOP::AMIGA_NONE))) {
            return amiga_change(Self, Channel);
         }

         // No sample change - we are finished
         Self->finish(Channel, true);
         return true;
      }
      else return false;
   }

   if (lp_end <= lp_start) {
      Self->finish(Channel, true);
      return true;
   }

   if ((Channel.Flags & CHF::BACKWARD) != CHF::NIL) {
      if (lp_type IS LTYPE::BIDIRECTIONAL) {
         if (Channel.Position < lp_start - 1) fold_bidirectional(Channel, lp_start, lp_end);
      }
      else if (Channel.Position < lp_start) {
         const int64_t length = lp_end - lp_start;
         Channel.Position = lp_start + ((Channel.Position - lp_start) % length + length) % length;
         Channel.LoopHistory = true;
      }
   }
   else if (Channel.Position >= lp_end) {
      Channel.LoopHistory = true; // Going forward - did we reach loop end?
      if (sample.Stream) {
         // Read the next set of stream data into our sample buffer
         BYTELEN bytes_read = fill_stream_buffer(Channel.SampleHandle, sample, -1);
         bytes_read = BYTELEN(bytes_read - (bytes_read % sample.FrameBytes)); // Incomplete frames are discarded
         if (sample.Swap) swap_samples_16(sample.Data.data(), bytes_read);
         sample.BufferedLength = bytes_read;
         auto buffer_len = int64_t(sample.SampleLength) * sample.FrameBytes;
         if (bytes_read < buffer_len) {
            clearmem(sample.Data.data() + bytes_read, buffer_len - bytes_read);
         }

         if ((bytes_read <= 0) or
             (sample.StreamLengthKnown and sample.PlayPos >= sample.StreamLength)) {
            // Loop back to the beginning if the client has defined a loop.  Otherwise finish.
            if (sample.streamLoops()) {
               sample.PlayPos = BYTELEN(0);
               sample.BufferedLength = BYTELEN(0);
            }
            else Self->finish(Channel, true);
         }
         else sample.PlayPos += bytes_read;
      }

      // Check for ALE sample change

      auto &prev_sample = Self->Samples[Channel.SampleHandle-1];

      if (((Channel.Flags & CHF::CHANGED) != CHF::NIL) and
          ((sample.LoopMode IS LOOP::AMIGA) or (sample.LoopMode IS LOOP::AMIGA_NONE)) and
          ((prev_sample.LoopMode IS LOOP::AMIGA) or (prev_sample.LoopMode IS LOOP::AMIGA_NONE))) {
         return amiga_change(Self, Channel);
      }

      // Go to the second loop if the sound has been released

      if ((Channel.LoopIndex IS 1) and (Channel.State IS CHS::RELEASED)) {
         Channel.LoopIndex = 2;
         return false;
      }

      if (lp_type IS LTYPE::BIDIRECTIONAL) fold_bidirectional(Channel, lp_start, lp_end);
      else Channel.Position = lp_start + (Channel.Position - lp_end) % (lp_end - lp_start);
   }

   return false;
}

//********************************************************************************************************************
// Main entry point for mixing sound data to destination

[[maybe_unused]] static ERR mix_data(extAudio *Self, int Elements, APTR Dest)
{
   AudioLog log(__FUNCTION__);

   while (Elements > 0) {
      Self->snapshot_quality();

      // Mix only as much as we can fit in our mixing buffer

      auto window = (Elements > Self->MixElements) ? Self->MixElements : Elements;

      // Clear the mix buffer, then mix all channels to the buffer

      int window_size = sizeof(float) * (Self->Stereo ? (window<<1) : window);
      clearmem(Self->MixBuffer.data(), window_size);

      int source_frames = 0;
      bool source_active = false;
      bool upstream_pending = false;
      uint64_t upstream_bound = 0;
      for (auto n=1; n < (int)Self->Sets.size(); n++) {
         auto &set = Self->Sets[n];
         const bool effects = set.Effects and !set.Effects->Effects.empty();
         auto buffer = effects ? set.ScratchBuffer.data() : Self->MixBuffer.data();
         if (effects) clearmem(buffer, window_size);

         Self->SourceFrames = 0;
         for (auto &c : set.Channel) {
            if (c.active()) mix_channel(Self, c, window, buffer);
         }

         for (auto &c : set.Shadow) {
            if (c.active()) mix_channel(Self, c, window, buffer);
         }

         bool set_active = false;
         for (auto &channel : set.Channel) set_active |= channel.active() and !channel.isStopped();
         for (auto &channel : set.Shadow) set_active |= channel.active() and !channel.isStopped();
         source_active |= set_active;
         source_frames = std::max(source_frames, Self->SourceFrames);

         if (effects) {
            const bool was_pending = set.Effects->pending();
            upstream_bound = std::max(upstream_bound, set.Effects->tail_bound());
            render_effects(*set.Effects, buffer, window, Self->SourceFrames,
               uint64_t(Self->MaxDrain * Self->OutputRate), set_active);
            upstream_pending |= was_pending or set.Effects->pending();
            for (int i = 0; i < window_size / int(sizeof(float)); ++i) Self->MixBuffer[i] += buffer[i];
         }
      }

      render_effects(*Self->GlobalEffects, Self->MixBuffer.data(), window, source_frames,
         uint64_t(Self->MaxDrain * Self->OutputRate), source_active, upstream_bound, upstream_pending);

      // Do optional post-processing

      if ((Self->WindowFlags & (ADF::FILTER_LOW|ADF::FILTER_HIGH)) != ADF::NIL) {
         if (Self->Stereo) filter_float_stereo(Self, Self->MixBuffer.data(), window);
         else filter_float_mono(Self, Self->MixBuffer.data(), window);
      }

      // Convert the floating point data to the correct output format

      if (Self->BitDepth IS 32) { // Presumes a floating point target identical to our own
         convert_float(Self->MixBuffer.data(), (Self->Stereo) ? window<<1 : window, (float *)Dest);
      }
      else if (Self->BitDepth IS 24) {

      }
      else if (Self->BitDepth IS 16) {
         convert_float16(Self, Self->MixBuffer.data(), (Self->Stereo) ? window<<1 : window, (int16_t *)Dest);
      }
      else convert_float8(Self, Self->MixBuffer.data(),  (Self->Stereo) ? window<<1 : window, (uint8_t *)Dest);

      Dest = ((uint8_t *)Dest) + (window * Self->DriverBitSize);
      Elements -= window;
   }

   return ERR::Okay;
}

//********************************************************************************************************************

#ifdef AUDIO_WORKER
// Consume only complete source frames.  Missing source data leaves this channel silent without advancing it.
static void mix_stream(extAudio *Self, AudioChannel &Channel, AudioSample &Sample, int TotalSamples, float *Dest)
{
   const int frame_bytes = Sample.FrameBytes;
   const int channels = Self->Stereo ? 2 : 1;
   const int64_t step = (int64_t(Channel.Frequency) << 16) / Self->OutputRate;
   if (step <= 0 or !Sample.Prefilled) return;
   for (int i = 0; i < TotalSamples; ++i) {
      const int64_t position = Channel.PositionLow + step;
      const size_t advance = size_t(position >> 16) * frame_bytes;
      const auto mode = Self->WindowConfig.resampler;
      const int support = mode IS ResamplerMode::CUBIC or Channel.Resampler.Weights[2] != 0 ? 3 :
         (mode IS ResamplerMode::LINEAR or Channel.Resampler.Weights[1] != 0 ? 2 : 1);
      const size_t needed = std::max(size_t(frame_bytes * support), advance);

      if (Sample.Ring.Used < needed and !Sample.EndOfSource) {
         if (!Sample.Starved) { ++Self->Starvations; Sample.Starved = true; }
         request_stream(Self, Sample);
         return;
      }

      if (!Sample.Ring.Used) {
         if (Sample.EndOfSource) Self->finish(Channel, true);
         else request_stream(Self, Sample);
         return;
      }

      // Retain the preceding consumed frame and gather look-ahead across the physical ring wrap.
      // EOF repeats the last available frame; starvation waits for support without moving the cursor.
      alignas(4) uint8_t frames[16];
      for (int tap = 0; tap < 4; ++tap) {
         for (int byte = 0; byte < frame_bytes; ++byte) {
            const size_t offset = std::min(size_t(std::max(0, tap - 1)),
               Sample.Ring.Used / frame_bytes - 1) * frame_bytes;
            frames[tap * frame_bytes + byte] = tap IS 0 and Sample.HasHistory ? Sample.History[byte] :
               Sample.Data[(Sample.Ring.Read + offset + byte) % Sample.Data.size()];
         }
      }

      const bool source_stereo = pcm_stereo(Sample.SampleType);
      const double volume = Self->Mute ? 0 :
         Self->MasterVolume * ((!Self->Stereo and source_stereo) ? 0.5 : 1.0);
      auto dest = Dest + i * channels;
      set_mix_step(int(step));

      MixingParams params = {
         .src = frames, .src_pos = 65536 + Channel.PositionLow, .total_samples = 1,
         .left_vol = float(volume * Channel.LVolume), .right_vol = float(volume * Channel.RVolume),
         .mix_dest = &dest, .length = 4, .transition = &Channel.Resampler,
               .loop_history = Channel.LoopHistory or (Channel.Flags & CHF::BACKWARD) != CHF::NIL
      };

      const float previous_left = dest[0];
      const float previous_right = Self->Stereo ? dest[1] : 0;
      AudioMixer::dispatch_mix(Self->WindowConfig, Sample.SampleType, params);
      const double cache_master = Self->MasterVolume > 0 ? Self->MasterVolume : 1;
      Channel.LastLeft = float((dest[-channels] - previous_left) / cache_master);
      Channel.LastRight = Self->Stereo ? float((dest[-1] - previous_right) / cache_master) : 0;
      Self->SourceFrames = std::max(Self->SourceFrames, i + 1);
      if ((Channel.Flags & CHF::VOL_RAMP) != CHF::NIL) {
         const bool left = adjust_volume_ramp(Channel.LVolume, Channel.LVolumeTarget, glRampStep);
         const bool right = adjust_volume_ramp(Channel.RVolume, Channel.RVolumeTarget, glRampStep);
         if (!left and !right) Channel.Flags &= ~CHF::VOL_RAMP;
      }

      const size_t consumed = std::min(advance, Sample.Ring.Used);
      if (consumed >= size_t(frame_bytes)) {
         for (int byte = 0; byte < frame_bytes; ++byte) {
            Sample.History[byte] = Sample.Data[(Sample.Ring.Read + consumed - frame_bytes + byte) % Sample.Data.size()];
         }
         Sample.HasHistory = true;
      }
      Sample.Ring.consume(consumed, Sample.Data.size(), frame_bytes);
      int64_t play_pos = int64_t(Sample.PlayPos) + consumed;
      if (Sample.StreamLengthKnown and Sample.streamLoops() and play_pos >= Sample.StreamLength) {
         const int64_t start = int64_t(Sample.Loop2Start) * Sample.FrameBytes;
         if (Sample.StreamLength > start) {
            play_pos = start + (play_pos - Sample.StreamLength) % (Sample.StreamLength - start);
         }
         else Sample.Released = true; // A degenerate loop ends this playback but keeps the configuration.
      }

      Sample.PlayPos = BYTELEN(play_pos);
      Sample.BufferedLength = BYTELEN(Sample.Ring.Used);
      Channel.PositionLow = position & 0xffff;
      if (Sample.Ring.Used <= Sample.Data.size() / 2) request_stream(Self, Sample);
   }
}
#endif

static void mix_channel(extAudio *Self, AudioChannel &Channel, int TotalSamples, APTR Dest)
{
   AudioLog log(__FUNCTION__);

   if (Channel.isStopped()) return;

   // A stream cannot share its consuming ring with a shadow.  A held output frame also folds a busy shadow
   // into a rapid retrigger without discarding its audible contribution.  Storage belongs to the channel set.
   if (Channel.HeldFrames) {
      auto dest = (float *)Dest;
      const int channels = Self->Stereo ? 2 : 1;
      const int frames = std::min(TotalSamples, Channel.HeldFrames);
      for (int i = 0; i < frames; ++i) {
         const double gain = double(Channel.HeldFrames) / glRampFrames;
         Channel.LastLeft = float(Channel.HeldLeft * gain);
         Channel.LastRight = float(Channel.HeldRight * gain);
         const double master = Self->Mute ? 0 : Self->MasterVolume;
         dest[i * channels] += float(Channel.LastLeft * master);
         if (Self->Stereo) dest[i * channels + 1] += float(Channel.LastRight * master);
         --Channel.HeldFrames;
      }
      Self->SourceFrames = std::max(Self->SourceFrames, frames);
      if (!Channel.HeldFrames) {
         Channel.LastLeft = Channel.LastRight = 0;
         Channel.State = CHS::FINISHED;
         Channel.Flags &= ~CHF::VOL_RAMP;
      }
      return;
   }

   auto &sample = Self->Samples[Channel.SampleHandle];

   // Check that we have something to mix

   if ((!Channel.Frequency) or (sample.Data.empty()) or (sample.SampleLength <= 0)) return;

#ifdef AUDIO_WORKER
   if (sample.Stream) {
      if (!Channel.isStopped()) mix_stream(Self, Channel, sample, TotalSamples, (float *)Dest);
      return;
   }
#endif

   // Calculate resampling step (16.16 fixed point)

   const int step = int(int64_t(Channel.Frequency) * 65536 / Self->OutputRate);

   if (step <= 0) return;

   const int sample_size = sample.FrameBytes;
   const bool sample_is_stereo = pcm_stereo(sample.SampleType);

   // Calculate stereo multiplier using template information
   const double stereo_mul = (!Self->Stereo and sample_is_stereo) ? 0.5 : 1.0;
   double mastervol = Self->Mute ? 0 : Self->MasterVolume * stereo_mul;
   const double cache_master = Self->MasterVolume > 0 ? Self->MasterVolume : 1;

   const int requested_frames = TotalSamples;
   float *mix_dest = (float *)Dest;
   while (TotalSamples > 0) {
      if (Channel.isStopped()) return;

      int sue = samples_until_end(Self, Channel);

      // Calculate the number of destination samples (note rounding)

      int mix_until_end = sue / step;
      if (sue % step) mix_until_end++;

      int mix_now = (mix_until_end > TotalSamples) ? TotalSamples : mix_until_end;
      TotalSamples -= mix_now;

      if (mix_now > 0) {
         Self->SourceFrames = std::max(Self->SourceFrames, requested_frames - TotalSamples);
         if (Channel.PositionLow < 0) { // Sanity check
            log.warning("Detected invalid PositionLow value of %d", Channel.PositionLow);
            return;
         }

         const int64_t source_origin = std::max(int64_t(0), Channel.Position);
         int mix_pos = int((Channel.Position - source_origin) * 65536) + Channel.PositionLow;
         auto mix_sample = sample.Data.data() + (sample_size * source_origin);

         // The existing step is thread-local; source timing remains 16.16 fixed point.

         const int mix_step = ((Channel.Flags & CHF::BACKWARD) != CHF::NIL) ? -step : step;
         set_mix_step(mix_step);

         while (mix_now > 0) {
            const bool ramp = (Channel.Flags & CHF::VOL_RAMP) != CHF::NIL;
            const int frames = ramp ? 1 : mix_now;
            MixingParams params = {
               .src = mix_sample, .src_pos = mix_pos, .total_samples = frames,
               .left_vol = float(mastervol * Channel.LVolume),
               .right_vol = float(mastervol * Channel.RVolume),
               .mix_dest = &mix_dest,
               .origin = source_origin, .length = sample.SampleLength,
               .loop_start = Channel.LoopIndex IS 2 ? sample.Loop2Start : sample.Loop1Start,
               .loop_end = Channel.LoopIndex IS 2 ? sample.Loop2End : sample.Loop1End,
               .loop_type = Channel.LoopIndex IS 2 ? sample.Loop2Type : sample.Loop1Type,
               .transition = &Channel.Resampler,
               .loop_history = Channel.LoopHistory or (Channel.Flags & CHF::BACKWARD) != CHF::NIL
            };
            auto frame = mix_dest + (frames - 1) * (Self->Stereo ? 2 : 1);
            const float previous_left = frame[0];
            const float previous_right = Self->Stereo ? frame[1] : 0;
            mix_pos = AudioMixer::dispatch_mix(Self->WindowConfig, sample.SampleType, params);
            Channel.LastLeft = float((frame[0] - previous_left) / cache_master);
            Channel.LastRight = Self->Stereo ? float((frame[1] - previous_right) / cache_master) : 0;
            mix_now -= frames;
            if (ramp) {
               const bool left = adjust_volume_ramp(Channel.LVolume, Channel.LVolumeTarget, glRampStep);
               const bool right = adjust_volume_ramp(Channel.RVolume, Channel.RVolumeTarget, glRampStep);
               if (!left and !right) Channel.Flags &= ~CHF::VOL_RAMP;
            }
         }

         // Save state back to channel structure
         Channel.PositionLow = mix_pos & 0xffff;
         Channel.Position = (mix_pos>>16) + ((mix_sample - sample.Data.data()) / sample_size);
         if (Channel.LVolume IS 0 and Channel.RVolume IS 0 and Channel.State IS CHS::FADE_OUT) {
            Self->finish(Channel, true);
            Channel.Flags &= ~CHF::VOL_RAMP;
         }
      }
      else if (mix_now < 0) {
         log.warning("Detected invalid mix values; TotalSamples: %d, MixNow: %d, SUE: %d, Step: %d, "
            "ChannelPos: %" PF64, TotalSamples, mix_now, sue, step, (long long)Channel.Position);
         return;
      }

      // Check if we reached loop/sample/whatever end

      if (handle_sample_end(Self, Channel)) return;
   }
}

//********************************************************************************************************************
// Mono output filtering routines.

static void filter_float_mono(extAudio *Self, float *Data, int TotalSamples)
{
#ifdef AUDIO_WORKER
   auto &d1l = Self->FilterHistory[0];
   auto &d2l = Self->FilterHistory[1];
#else
   static float d1l=0, d2l=0;
#endif

   if ((Self->WindowFlags & ADF::FILTER_LOW) != ADF::NIL) {
      while (TotalSamples > 0) {
         float s = (d1l + 2.0f * Data[0]) * (1.0f / 3.0f);
         d1l = Data[0];
         *(Data++) = s;
         TotalSamples--;
      }
   }
   else if ((Self->WindowFlags & ADF::FILTER_HIGH) != ADF::NIL) {
      while (TotalSamples > 0) {
         float s = (d1l + 3.0f * d2l + 4.0f * Data[0]) * (1.0f / 8.0f);
         d1l = d2l;
         d2l = Data[0];
         *(Data++) = s;
         TotalSamples--;
      }
   }
}

//********************************************************************************************************************
// Stereo output filtering routines.

static void filter_float_stereo(extAudio *Self, float *Data, int TotalSamples)
{
#ifdef AUDIO_WORKER
   auto &d1l = Self->FilterHistory[0];
   auto &d2l = Self->FilterHistory[1];
   auto &d1r = Self->FilterHistory[2];
   auto &d2r = Self->FilterHistory[3];
#else
   static double d1l = 0, d1r = 0, d2l = 0, d2r = 0;
#endif

   if ((Self->WindowFlags & ADF::FILTER_LOW) != ADF::NIL) {
      while (TotalSamples > 0) {
         float s = (d1l + 2.0 * Data[0]) * (1.0 / 3.0);
         d1l = Data[0];
         *(Data++) = s;

         s = (d1r + 2.0 * Data[0]) * (1.0 / 3.0);
         d1r = Data[0];
         *(Data++) = s;

         TotalSamples--;
      }
   }
   else if ((Self->WindowFlags & ADF::FILTER_HIGH) != ADF::NIL) {
      while (TotalSamples > 0) {
         float s = (d1l + 3.0 * d2l + 4.0 * Data[0]) * (1.0 / 8.0);
         d1l = d2l;
         d2l = Data[0];
         *(Data++) = s;

         s = (d1r + 3.0 * d2r + 4.0 * Data[0]) * (1.0 / 8.0);
         d1r = d2r;
         d2r = Data[0];
         *(Data++) = s;

         TotalSamples--;
      }
   }
}
