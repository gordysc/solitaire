#include "audio.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <numbers>
#include <random>
#include <vector>

/// @brief Everything that belongs to the Solitaire game.
namespace solitaire {

/// @brief Sound synthesis helpers internal to `audio.cpp`.
///
/// Every sound is built the same way: work out each sample's value with a
/// little maths (sine waves, filtered noise, envelopes), store the samples
/// in a vector, and hand them to raylib. Card sounds are mostly short
/// bursts of noise: paper hitting felt has no clear pitch.
namespace {

/// @brief Samples per second.
constexpr int kSampleRate = 44100;
/// @brief A full turn in radians, for oscillator phase.
constexpr double kTwoPi = 2.0 * std::numbers::pi;

/// @brief Semitone steps of a major scale, one per rank (Ace to King), so a
/// foundation built from Ace up plays a rising scale.
constexpr std::array<int, 13> kMajorScale{0,  2,  4,  5,  7,  9, 11,
                                          12, 14, 16, 17, 19, 21};

/// @brief Builds an empty sample buffer for a sound `seconds` long.
/// @param seconds Length of the sound.
/// @return Zero-filled buffer.
std::vector<float> Buffer(double seconds) {
  return std::vector<float>(static_cast<std::size_t>(seconds * kSampleRate),
                            0.0f);
}

/// @brief Triangle wave: softer than a square wave, brighter than a sine.
/// @param phase Phase in radians.
/// @return Sample in `[-1, 1]`.
double Triangle(double phase) {
  const double t = std::fmod(phase / kTwoPi, 1.0);
  return 4.0 * std::abs(t - 0.5) - 1.0;
}

/// @brief Adds a decaying tone to `out`, starting `delay` seconds in.
/// @param out Buffer to mix into (passed by reference and changed).
/// @param delay Start time in seconds.
/// @param freq Frequency in Hz.
/// @param decay How fast it fades; higher = shorter.
/// @param volume Peak volume.
/// @param triangle True for a triangle wave, false for a sine wave.
void AddTone(std::vector<float>& out, double delay, double freq, double decay,
             double volume, bool triangle) {
  const auto first = static_cast<std::size_t>(delay * kSampleRate);
  for (std::size_t i = first; i < out.size(); ++i) {
    const double t = static_cast<double>(i - first) / kSampleRate;
    const double phase = kTwoPi * freq * t;
    // A 4 ms fade-in stops the click a sudden start would make.
    const double attack = std::min(1.0, t / 0.004);
    const double wave = triangle ? Triangle(phase) : std::sin(phase);
    out[i] += static_cast<float>(wave * volume * attack * std::exp(-decay * t));
  }
}

/// @brief Adds a burst of filtered noise to `out`.
///
/// `tone` sets the filter: each new sample moves only part of the way from
/// the last sample towards a fresh random value (a "one-pole low-pass
/// filter"). A small `tone` smooths out the hiss into a soft rustle; a
/// `tone` near 1 keeps it crisp, like a card snapping down.
/// @param out Buffer to mix into.
/// @param rng Random engine (by reference, so its state advances).
/// @param delay Start time in seconds.
/// @param attack Seconds to fade in.
/// @param decay How fast it fades; higher = shorter.
/// @param volume Peak volume.
/// @param tone Filter amount from 0 (dull) to 1 (bright).
void AddNoise(std::vector<float>& out, std::mt19937& rng, double delay,
              double attack, double decay, double volume, float tone) {
  std::uniform_real_distribution<float> noise(-1.0f, 1.0f);
  const auto first = static_cast<std::size_t>(delay * kSampleRate);
  float smooth = 0.0f;
  for (std::size_t i = first; i < out.size(); ++i) {
    const double t = static_cast<double>(i - first) / kSampleRate;
    smooth += (noise(rng) - smooth) * tone;
    const double rise = attack > 0.0 ? std::min(1.0, t / attack) : 1.0;
    out[i] += static_cast<float>(smooth * volume * rise * std::exp(-decay * t));
  }
}

/// @brief Synthesises the samples for one sound effect.
/// @param effect Which sound.
/// @return Mono samples in roughly `[-1, 1]`.
std::vector<float> Synthesize(SoundEffect effect) {
  // A fixed seed: the noise in each sound is the same every run.
  std::mt19937 rng(11U);
  switch (effect) {
    case SoundEffect::Pick: {
      auto out = Buffer(0.06);
      AddNoise(out, rng, 0.0, 0.002, 90.0, 0.25, 0.45f);
      return out;
    }
    case SoundEffect::Place: {
      auto out = Buffer(0.14);
      AddNoise(out, rng, 0.0, 0.0, 60.0, 0.55, 0.75f);
      AddTone(out, 0.0, 150.0, 45.0, 0.25, false);
      return out;
    }
    case SoundEffect::Draw: {
      auto out = Buffer(0.18);
      AddNoise(out, rng, 0.0, 0.03, 28.0, 0.35, 0.25f);
      return out;
    }
    case SoundEffect::Flip: {
      auto out = Buffer(0.09);
      AddNoise(out, rng, 0.0, 0.0, 120.0, 0.3, 0.8f);
      AddNoise(out, rng, 0.035, 0.0, 120.0, 0.22, 0.6f);
      return out;
    }
    case SoundEffect::Foundation: {
      auto out = Buffer(0.6);
      AddNoise(out, rng, 0.0, 0.0, 70.0, 0.4, 0.75f);
      AddTone(out, 0.0, 523.25, 7.0, 0.22, false);
      AddTone(out, 0.0, 1046.5, 12.0, 0.06, false);
      return out;
    }
    case SoundEffect::Invalid: {
      auto out = Buffer(0.25);
      AddTone(out, 0.0, 110.0, 16.0, 0.3, true);
      AddTone(out, 0.07, 92.5, 16.0, 0.3, true);
      return out;
    }
    case SoundEffect::Deal: {
      // A quick run of card ticks with slightly uneven spacing, like a
      // real deal.
      auto out = Buffer(0.9);
      std::uniform_real_distribution<double> jitter(-0.006, 0.006);
      for (int i = 0; i < 24; ++i) {
        AddNoise(out, rng, 0.03 * i + 0.01 + jitter(rng), 0.0, 150.0, 0.2,
                 0.7f);
      }
      return out;
    }
    case SoundEffect::Undo: {
      // Noise that swells and stops: a sound played backwards.
      auto out = Buffer(0.2);
      std::uniform_real_distribution<float> noise(-1.0f, 1.0f);
      float smooth = 0.0f;
      for (std::size_t i = 0; i < out.size(); ++i) {
        const double u =
            static_cast<double>(i) / static_cast<double>(out.size());
        smooth += (noise(rng) - smooth) * 0.3f;
        out[i] = static_cast<float>(smooth * 0.35 * u * u);
      }
      return out;
    }
    case SoundEffect::Hint: {
      auto out = Buffer(0.45);
      AddTone(out, 0.0, 1046.5, 12.0, 0.18, false);
      AddTone(out, 0.09, 1318.5, 10.0, 0.18, false);
      return out;
    }
    case SoundEffect::Win: {
      auto out = Buffer(2.0);
      constexpr std::array<double, 7> kSteps{0, 4, 7, 12, 16, 19, 24};
      for (std::size_t n = 0; n < kSteps.size(); ++n) {
        const double freq = 392.0 * std::pow(2.0, kSteps[n] / 12.0);
        AddTone(out, 0.09 * static_cast<double>(n), freq, 4.0, 0.14, true);
      }
      for (const double step : {12.0, 16.0, 19.0}) {
        AddTone(out, 0.75, 392.0 * std::pow(2.0, step / 12.0), 1.6, 0.12,
                false);
      }
      return out;
    }
  }
  return Buffer(0.01);
}

/// @brief Converts float samples to a raylib Sound.
/// @param samples Mono samples in roughly `[-1, 1]`.
/// @return A loaded Sound (owns its own copy of the samples).
Sound MakeSound(const std::vector<float>& samples) {
  // raylib plays 16-bit integers. Clamp first so a loud mix clips cleanly
  // instead of wrapping around (which would be a nasty crackle).
  std::vector<std::int16_t> pcm(samples.size());
  std::ranges::transform(samples, pcm.begin(), [](float s) {
    return static_cast<std::int16_t>(std::clamp(s, -1.0f, 1.0f) * 32767.0f);
  });
  Wave wave{};
  wave.frameCount = static_cast<unsigned int>(pcm.size());
  wave.sampleRate = kSampleRate;
  wave.sampleSize = 16;
  wave.channels = 1;
  // raylib's C struct `Wave` has a non-const `void* data`, but
  // `LoadSoundFromWave` only reads it and copies the samples into the audio
  // device's own buffer. So `pcm` keeps ownership and frees itself (RAII)
  // when this function returns, after raylib has made its copy.
  wave.data = pcm.data();
  return LoadSoundFromWave(wave);
}

}  // namespace

Audio::Audio() {
  if (!IsAudioDeviceReady()) return;
  for (int i = 0; i < kSoundEffectCount; ++i) {
    Bank& bank = banks_[static_cast<std::size_t>(i)];
    bank.base = MakeSound(Synthesize(static_cast<SoundEffect>(i)));
    for (Sound& voice : bank.voices) voice = LoadSoundAlias(bank.base);
  }
  loaded_ = true;
}

Audio::~Audio() {
  if (!loaded_) return;
  for (Bank& bank : banks_) {
    // Aliases share `base`'s samples, so unload them first; unloading the
    // base first would leave the aliases pointing at freed memory.
    for (Sound& voice : bank.voices) UnloadSoundAlias(voice);
    UnloadSound(bank.base);
  }
}

void Audio::Play(SoundEffect effect, float pitch) {
  if (!loaded_ || muted_) return;
  Bank& bank = banks_[static_cast<std::size_t>(effect)];
  Sound& voice = bank.voices[static_cast<std::size_t>(bank.next)];
  bank.next = (bank.next + 1) % kVoices;
  SetSoundPitch(voice, std::clamp(pitch, 0.25f, 4.0f));
  PlaySound(voice);
}

void Audio::PlayFoundation(int rank) {
  const int step = std::clamp(rank - 1, 0, 12);
  // Pitch is a speed ratio: 2^(semitones / 12) moves up that many semitones.
  const float pitch = std::pow(
      2.0f,
      static_cast<float>(kMajorScale[static_cast<std::size_t>(step)]) / 12.0f);
  Play(SoundEffect::Foundation, pitch);
}

}  // namespace solitaire
