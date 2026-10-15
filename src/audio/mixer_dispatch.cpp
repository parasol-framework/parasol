#include "mixer_dispatch.h"

// All kernels repeat ordinary endpoints and extend logical loops. Ordinary ends repeat their endpoint; loops wrap or
// reflect with duplicated endpoints (e.g. ...2,3,3,2...). Reverse playback uses the same spatial curve.
static int64_t resampler_neighbour(int64_t Index, const MixingParams &Params)
{
   if (Params.loop_type != LTYPE::NIL and Params.loop_end > Params.loop_start and
       (Index >= Params.loop_end or (Index < Params.loop_start and Params.loop_history))) {
      const int64_t length = Params.loop_end - Params.loop_start;
      const int64_t period = Params.loop_type IS LTYPE::BIDIRECTIONAL ? 2 * length : length;
      int64_t offset = (Index - Params.loop_start) % period;
      if (offset < 0) offset += period;
      if (offset >= length) offset = period - 1 - offset;
      Index = Params.loop_start + offset;
   }
   return std::clamp(Index, int64_t(0), Params.length - 1) - Params.origin;
}

#include "resampler_sinc.h"

// Catmull-Rom cubic Hermite: m0=(y1-ym1)/2, m1=(y2-y0)/2;
// p(t)=y0+t*(m0+t*(3*(y1-y0)-2*m0-m1+t*(2*(y0-y1)+m0+m1))).
// t is the unsigned 16.16 fraction. The four-point convex hull bounds the result, preserving PCM headroom
// without a final-output limiter. Exact positions reproduce y0; double evaluation rounds once into float.
static double resampler_cubic(double Before, double Current, double Next, double After, double Phase)
{
   const double m0 = (Next - Before) * 0.5;
   const double m1 = (After - Current) * 0.5;
   const double value = Current + Phase * (m0 + Phase * (3 * (Next - Current) - 2 * m0 - m1 +
      Phase * (2 * (Current - Next) + m0 + m1)));
   return std::clamp(value, std::min({Before, Current, Next, After}), std::max({Before, Current, Next, After}));
}

static void resampler_transition(ResamplerState &State, ResamplerMode Mode)
{
   if (!State.Initialised) {
      State.Initialised = true;
      State.Target = Mode;
      for (int i = 0; i < 4; ++i) State.Weights[i] = i IS int(Mode) ? 1 : 0;
   }
   if (State.Target != Mode) {
      State.Target = Mode;
      std::copy_n(State.Weights, 4, State.Start);
      State.Remaining = 32;
   }
   if (State.Remaining) {
      const double phase = double(33 - State.Remaining) / 32;
      for (int i = 0; i < 4; ++i) {
         State.Weights[i] = State.Start[i] * (1 - phase) + (i IS int(Mode) ? phase : 0);
      }
      --State.Remaining;
   }
}

template<typename T, bool Stereo>
static int resampler_mix(const AudioConfig &Config, const MixingParams &Params)
{
   auto source = (const T *)Params.src;
   auto dest = *Params.mix_dest;
   int position = Params.src_pos;
   constexpr int channels = Stereo ? 2 : 1;
   const double ratio = std::abs(double(glMixStep)) / 65536;
   // Reserve 0.05 of source Nyquist for the finite transition band when downsampling.
   const double cutoff = ratio <= 1 ? 1 : std::max(0.0, 1.0 / ratio - 0.05);
   const double bank = cutoff * glSincCutoffs;
   const int lower = std::min(int(bank), glSincCutoffs - 1);
   const double blend = bank - lower;
   for (int frame = 0; frame < Params.total_samples; ++frame) {
      if (Params.transition) resampler_transition(*Params.transition, Config.resampler);
      const int base = position >> 16;
      const double phase = double(position & 0xffff) / 65536;
      double values[2];
      for (int channel = 0; channel < channels; ++channel) {
         auto read = [&](int Offset) {
            const auto index = resampler_neighbour(Params.origin + base + Offset, Params);
            return SampleTraits<T>::normalize(source[index * channels + channel]);
         };
         const double current = read(0);
         auto weight = [&](ResamplerMode Mode) {
            return Params.transition ? Params.transition->Weights[int(Mode)] : double(Config.resampler IS Mode);
         };
         const double linear_weight = weight(ResamplerMode::LINEAR);
         const double cubic_weight = weight(ResamplerMode::CUBIC);
         values[channel] = current * weight(ResamplerMode::NEAREST);
         if (linear_weight or cubic_weight) {
            const double next = read(1);
            values[channel] += linear_weight * (current + phase * (next - current));
            if (cubic_weight) {
               values[channel] += cubic_weight * resampler_cubic(read(-1), current, next, read(2), phase);
            }
         }
         const double sinc_weight = weight(ResamplerMode::SINC);
         if (sinc_weight) {
            const double phase_index = phase * glSincPhases;
            const int row = int(phase_index);
            const double fraction = phase_index - row;
            double value = 0;
            for (int tap = 0; tap < glSincTaps; ++tap) {
               const double a = glSincTable.Coefficients[lower][row][tap];
               const double b = glSincTable.Coefficients[lower][row + 1][tap];
               const double c = glSincTable.Coefficients[lower + 1][row][tap];
               const double d = glSincTable.Coefficients[lower + 1][row + 1][tap];
               const double first = a + fraction * (b - a);
               const double second = c + fraction * (d - c);
               const auto index = sinc_neighbour(Params.origin + base + tap - glSincPast, Params);
               value += (first + blend * (second - first)) *
                  SampleTraits<T>::normalize(source[index * channels + channel]);
            }
            values[channel] += sinc_weight * value;
         }
      }
      if (Config.stereo_output) {
         dest[0] += Params.left_vol * values[0];
         dest[1] += Params.right_vol * values[Stereo ? 1 : 0];
         dest += 2;
      }
      else *(dest++) += Params.left_vol * (values[0] + (Stereo ? values[1] : 0));
      position += glMixStep;
   }
   *Params.mix_dest = dest;
   return position;
}

int AudioMixer::dispatch_mix(const AudioConfig &Config, PCM Format, const MixingParams &Params)
{
   if (Params.length <= 0) return Params.src_pos;
   switch (Format) {
      case PCM::U8_MONO: return resampler_mix<uint8_t, false>(Config, Params);
      case PCM::U8_STEREO: return resampler_mix<uint8_t, true>(Config, Params);
      case PCM::S16_MONO: return resampler_mix<int16_t, false>(Config, Params);
      case PCM::S16_STEREO: return resampler_mix<int16_t, true>(Config, Params);
      default: return 0;
   }
}
