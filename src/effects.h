#ifndef SOLITAIRE_EFFECTS_H_
#define SOLITAIRE_EFFECTS_H_

#include <raylib.h>

#include <array>
#include <random>
#include <span>
#include <vector>

#include "card.h"

/// @brief Everything that belongs to the Solitaire game.
namespace solitaire {

/// @brief Moves every card smoothly towards where the rules say it is, and
/// flips cards over when they turn face up or face down.
///
/// The Klondike rules move cards instantly. Every frame, the Game tells the
/// animator where each card *should* be; the animator keeps where each card
/// *is* on screen and closes part of the gap each frame. That one idea
/// animates dealing, drawing, dropping, undo, and auto-complete alike.
class CardAnimator {
 public:
  /// @brief Seconds a card takes to flip over.
  static constexpr float kFlipSeconds = 0.22f;

  /// @brief Forgets every card, so the next `Follow()` call places each
  /// card straight at its target.
  void Reset();

  /// @brief Puts a card at `from` and keeps it there until `until`, then
  /// lets it fly to its target. Used to deal cards out one by one.
  /// @param card Card to hold.
  /// @param from Where it waits.
  /// @param face_up Which side it shows while waiting.
  /// @param until Animation clock time when it may start moving.
  void Hold(const Card& card, Vector2 from, bool face_up, float until);

  /// @brief Tells the animator where a card should be this frame.
  /// @param card The card; its `face_up` flag is the side it should show.
  /// @param target Top-left corner it should be at.
  /// @param snap True to jump there at once (a card being dragged).
  /// @param now Animation clock in seconds.
  /// @param dt Seconds since the last frame.
  void Follow(const Card& card, Vector2 target, bool snap, float now, float dt);

  /// @brief Returns where a card is drawn now.
  /// @param card The card.
  /// @return Top-left corner in pixels.
  Vector2 Position(const Card& card) const;

  /// @brief Returns how wide to draw a card while it flips: `1.0` normally,
  /// shrinking to `0.0` halfway through a flip (it is edge-on), then back.
  /// @param card The card.
  /// @param now Animation clock in seconds.
  /// @return Horizontal scale from 0 to 1.
  float FlipWidth(const Card& card, float now) const;

  /// @brief Returns which side of a card to draw now: the old side for the
  /// first half of a flip, the new side after.
  /// @param card The card.
  /// @param now Animation clock in seconds.
  /// @return True to draw the face.
  bool ShowsFace(const Card& card, float now) const;

  /// @brief Returns true while a card is travelling (or waiting to), so the
  /// renderer can draw it above the cards that are resting.
  /// @param card The card.
  /// @param now Animation clock in seconds.
  /// @return True if it is not yet at its target.
  bool InFlight(const Card& card, float now) const;

  /// @brief Returns true if any card is still travelling or flipping.
  /// @param now Animation clock in seconds.
  /// @return True while something is moving.
  bool AnyMoving(float now) const;

 private:
  /// @brief What the animator knows about one card.
  struct State {
    /// @brief False until the card is first seen.
    bool known = false;
    /// @brief Where it is drawn.
    Vector2 position{0.0f, 0.0f};
    /// @brief Where it is going.
    Vector2 target{0.0f, 0.0f};
    /// @brief Side it shows when settled.
    bool face_up = false;
    /// @brief Side it showed before the current flip.
    bool previous_face_up = false;
    /// @brief Animation clock time when the current flip started.
    float flip_start = -100.0f;
    /// @brief The card stays put until this time.
    float hold_until = -100.0f;
  };

  /// @brief One State per card, indexed by `CardId()`.
  std::array<State, kDeckSize> states_{};
};

/// @brief A card flying off the table in the win celebration.
struct FlyingCard {
  /// @brief Which card (always drawn face up).
  Card card;
  /// @brief Centre position in pixels.
  Vector2 position{0.0f, 0.0f};
  /// @brief Speed in pixels per second.
  Vector2 velocity{0.0f, 0.0f};
  /// @brief Rotation in degrees.
  float angle = 0.0f;
  /// @brief Rotation speed in degrees per second.
  float spin = 0.0f;
  /// @brief Animation clock time it takes off.
  float launch = 0.0f;
};

/// @brief One scrap of confetti.
struct Confetti {
  /// @brief Centre position in pixels.
  Vector2 position{0.0f, 0.0f};
  /// @brief Speed in pixels per second.
  Vector2 velocity{0.0f, 0.0f};
  /// @brief Rotation in degrees.
  float angle = 0.0f;
  /// @brief Rotation speed in degrees per second.
  float spin = 0.0f;
  /// @brief Seconds left to live.
  float life = 0.0f;
  /// @brief Colour.
  Color color = WHITE;
};

/// @brief The win celebration: every card leaps off its foundation in turn
/// and tumbles off the screen in a shower of confetti.
class Celebration {
 public:
  /// @brief Starts the celebration.
  /// @param cards Every card, with its starting centre position, in the
  ///        order they should take off.
  /// @param now Animation clock in seconds.
  void Start(const std::vector<FlyingCard>& cards, float now);

  /// @brief Stops and clears the celebration.
  void Clear();

  /// @brief Moves cards and confetti.
  /// @param dt Seconds since the last frame.
  /// @param now Animation clock in seconds.
  void Update(float dt, float now);

  /// @brief Returns true while anything is still on screen.
  /// @return True during the celebration.
  bool active() const { return !cards_.empty() || !confetti_.empty(); }

  /// @brief Cards still flying (some may not have taken off yet).
  /// @return Read-only view; valid until the next non-const call.
  std::span<const FlyingCard> cards() const { return cards_; }

  /// @brief Confetti still falling.
  /// @return Read-only view; valid until the next non-const call.
  std::span<const Confetti> confetti() const { return confetti_; }

  /// @brief Returns true once a card has left the table, so the renderer
  /// stops drawing it in its pile (even after it falls off the screen).
  /// @param card The card.
  /// @param now Animation clock in seconds.
  /// @return True if the card has taken off.
  bool Launched(const Card& card, float now) const {
    return now >= launch_times_[static_cast<std::size_t>(CardId(card))];
  }

 private:
  /// @brief Throws a burst of confetti from a point.
  /// @param at Burst centre.
  /// @param count Number of scraps.
  void Burst(Vector2 at, int count);

  /// @brief Cards still flying.
  std::vector<FlyingCard> cards_;
  /// @brief Confetti still falling.
  std::vector<Confetti> confetti_;
  /// @brief Take-off time of each card, indexed by `CardId()`. Cards that
  /// are not part of a celebration keep a time far in the future.
  std::array<float, kDeckSize> launch_times_ = NeverLaunched();

  /// @brief Returns a launch-time table where no card ever takes off.
  /// @return Every entry set to a huge time.
  static std::array<float, kDeckSize> NeverLaunched() {
    std::array<float, kDeckSize> times{};
    times.fill(1.0e30f);
    return times;
  }
  /// @brief Random source for directions and colours (looks only).
  std::mt19937 rng_{2024U};
};

}  // namespace solitaire

#endif  // SOLITAIRE_EFFECTS_H_
