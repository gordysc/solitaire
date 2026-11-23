#ifndef SOLITAIRE_AUDIO_H_
#define SOLITAIRE_AUDIO_H_

#include <raylib.h>

#include <array>
#include <cstdint>

/// @brief Everything that belongs to the Solitaire game.
namespace solitaire {

/// @brief Every sound the game makes. All of them are synthesised in code
/// at startup, so the game ships with no audio files.
enum class SoundEffect : std::uint8_t {
  /// @brief Soft papery tick when cards are picked up.
  Pick = 0,
  /// @brief Card snapping down onto the table.
  Place,
  /// @brief Card sliding off the stock.
  Draw,
  /// @brief Quick flick of a card turning over.
  Flip,
  /// @brief Bright chime when a card goes up to a foundation.
  Foundation,
  /// @brief Dull thud when a drop is not allowed.
  Invalid,
  /// @brief Riffle of a new deal.
  Deal,
  /// @brief Reverse swoosh of an undo.
  Undo,
  /// @brief Two-note ping that points out a hint.
  Hint,
  /// @brief Rising fanfare for a win.
  Win,
};

/// @brief Number of SoundEffect values, for sizing the sound bank.
inline constexpr int kSoundEffectCount = 10;

/// @brief Owns every synthesised sound and plays them on request.
///
/// RAII: the constructor builds and uploads the sounds, and the destructor
/// unloads them. Construct it after `InitAudioDevice()` and destroy it
/// before `CloseAudioDevice()`. If no audio device could be opened, every
/// method quietly does nothing, so the game still runs.
class Audio {
 public:
  /// @brief Synthesises every sound and uploads it to the audio device.
  Audio();

  /// @brief Unloads every sound.
  ~Audio();

  /// @brief Non-copyable: a copy would unload the same sounds twice.
  Audio(const Audio&) = delete;
  /// @brief Non-copy-assignable for the same reason.
  Audio& operator=(const Audio&) = delete;

  /// @brief Plays a sound.
  /// @param effect Sound to play.
  /// @param pitch Playback speed: `1.0` is normal, `2.0` an octave up.
  void Play(SoundEffect effect, float pitch = 1.0f);

  /// @brief Plays the foundation chime pitched by rank, so building a pile
  /// from Ace to King climbs a scale.
  /// @param rank Rank of the card that went up (1..13).
  void PlayFoundation(int rank);

  /// @brief Turns all sound off or on.
  /// @param muted True to silence the game.
  void set_muted(bool muted) { muted_ = muted; }
  /// @brief True if sound is off.
  bool muted() const { return muted_; }

 private:
  /// @brief Copies of each sound that can play at the same time, so quick
  /// moves don't cut each other off.
  static constexpr int kVoices = 4;

  /// @brief One sound and its extra voices.
  struct Bank {
    /// @brief The sound that owns the sample data.
    Sound base{};
    /// @brief Aliases: extra voices that share `base`'s samples instead of
    /// copying them (`LoadSoundAlias`). They must be unloaded before `base`.
    std::array<Sound, kVoices> voices{};
    /// @brief Voice to use next (round-robin).
    int next = 0;
  };

  /// @brief One bank per SoundEffect, indexed by its value.
  std::array<Bank, kSoundEffectCount> banks_{};
  /// @brief True if the sounds were loaded (an audio device was available).
  bool loaded_ = false;
  /// @brief True if sound is off.
  bool muted_ = false;
};

}  // namespace solitaire

#endif  // SOLITAIRE_AUDIO_H_
