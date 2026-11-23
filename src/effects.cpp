#include "effects.h"

#include <algorithm>
#include <cmath>
#include <numbers>

#include "layout.h"
#include "screen.h"

/// @brief Everything that belongs to the Solitaire game.
namespace solitaire {

/// @brief Animation constants internal to `effects.cpp`.
namespace {

/// @brief How quickly cards close the gap to their target. Each second the
/// remaining gap shrinks by a factor of e^16, so moves feel quick but soft.
constexpr float kFollowRate = 16.0f;
/// @brief Distance in pixels at which a card counts as arrived.
constexpr float kArrivedDistance = 0.5f;
/// @brief Downward pull on flying cards, in pixels per second squared.
constexpr float kGravity = 1500.0f;
/// @brief Seconds between two cards taking off in the celebration.
constexpr float kLaunchInterval = 0.05f;

/// @brief Returns the distance between two points.
/// @param a First point.
/// @param b Second point.
/// @return Distance in pixels.
float Distance(Vector2 a, Vector2 b) {
  return std::hypot(a.x - b.x, a.y - b.y);
}

}  // namespace

void CardAnimator::Reset() { states_.fill(State{}); }

void CardAnimator::Hold(const Card& card, Vector2 from, bool face_up,
                        float until) {
  State& s = states_[static_cast<std::size_t>(CardId(card))];
  s.known = true;
  s.position = from;
  s.target = from;
  s.face_up = face_up;
  s.previous_face_up = face_up;
  s.flip_start = -100.0f;
  s.hold_until = until;
}

void CardAnimator::Follow(const Card& card, Vector2 target, bool snap,
                          float now, float dt) {
  State& s = states_[static_cast<std::size_t>(CardId(card))];
  if (!s.known) {
    // First sighting: no animation, just put it there.
    s.known = true;
    s.position = target;
    s.face_up = card.face_up;
    s.previous_face_up = card.face_up;
  }
  s.target = target;
  if (card.face_up != s.face_up) {
    s.previous_face_up = s.face_up;
    s.face_up = card.face_up;
    // A held card flips when it is released, not while it waits.
    s.flip_start = std::max(now, s.hold_until);
  }
  if (now < s.hold_until) return;
  if (snap) {
    s.position = target;
    return;
  }
  // Exponential smoothing: move a fixed *fraction* of the remaining gap.
  // Basing the fraction on `dt` keeps the speed the same at any frame rate.
  const float follow = 1.0f - std::exp(-kFollowRate * dt);
  s.position.x += (target.x - s.position.x) * follow;
  s.position.y += (target.y - s.position.y) * follow;
  if (Distance(s.position, target) < kArrivedDistance) s.position = target;
}

Vector2 CardAnimator::Position(const Card& card) const {
  return states_[static_cast<std::size_t>(CardId(card))].position;
}

float CardAnimator::FlipWidth(const Card& card, float now) const {
  const State& s = states_[static_cast<std::size_t>(CardId(card))];
  const float t = (now - s.flip_start) / kFlipSeconds;
  if (t <= 0.0f || t >= 1.0f) return 1.0f;
  // `|cos|` goes 1 -> 0 -> 1 over half a turn: the card turns edge-on in
  // the middle of the flip, which is when the side being drawn changes.
  return std::abs(std::cos(std::numbers::pi_v<float> * t));
}

bool CardAnimator::ShowsFace(const Card& card, float now) const {
  const State& s = states_[static_cast<std::size_t>(CardId(card))];
  // Before a flip starts (t < 0) and during its first half, the old side
  // shows. A card that never flipped has a huge `t`, so it shows `face_up`.
  const float t = (now - s.flip_start) / kFlipSeconds;
  return t < 0.5f ? s.previous_face_up : s.face_up;
}

bool CardAnimator::InFlight(const Card& card, float now) const {
  const State& s = states_[static_cast<std::size_t>(CardId(card))];
  return now < s.hold_until || Distance(s.position, s.target) > 1.0f;
}

bool CardAnimator::AnyMoving(float now) const {
  // `std::ranges::any_of` is true if the lambda accepts any element.
  return std::ranges::any_of(states_, [now](const State& s) {
    return s.known &&
           (now < s.hold_until || now < s.flip_start + kFlipSeconds ||
            Distance(s.position, s.target) > 1.0f);
  });
}

void Celebration::Start(const std::vector<FlyingCard>& cards, float now) {
  Clear();
  std::uniform_real_distribution<float> sideways(-420.0f, 420.0f);
  std::uniform_real_distribution<float> upwards(-1000.0f, -560.0f);
  std::uniform_real_distribution<float> spin(-540.0f, 540.0f);
  for (std::size_t i = 0; i < cards.size(); ++i) {
    FlyingCard flying = cards[i];
    flying.velocity = {sideways(rng_), upwards(rng_)};
    flying.spin = spin(rng_);
    flying.angle = 0.0f;
    flying.launch = now + kLaunchInterval * static_cast<float>(i);
    launch_times_[static_cast<std::size_t>(CardId(flying.card))] =
        flying.launch;
    cards_.push_back(flying);
  }
}

void Celebration::Clear() {
  cards_.clear();
  confetti_.clear();
  launch_times_ = NeverLaunched();
}

void Celebration::Update(float dt, float now) {
  for (FlyingCard& card : cards_) {
    if (now < card.launch) continue;
    // The first frame after take-off: mark the spot with confetti. Setting
    // `launch` far in the past makes sure that happens only once.
    if (card.launch > -1000.0f) {
      Burst(card.position, 14);
      card.launch = -10000.0f;
    }
    card.velocity.y += kGravity * dt;
    card.position.x += card.velocity.x * dt;
    card.position.y += card.velocity.y * dt;
    card.angle += card.spin * dt;
  }
  for (Confetti& c : confetti_) {
    c.velocity.y += kGravity * 0.35f * dt;
    // Air drag: confetti floats rather than falls.
    const float drag = std::pow(0.25f, dt);
    c.velocity.x *= drag;
    c.velocity.y *= drag;
    c.position.x += c.velocity.x * dt;
    c.position.y += c.velocity.y * dt;
    c.angle += c.spin * dt;
    c.life -= dt;
  }
  // `std::erase_if` (C++20) removes every element the lambda accepts, in one
  // pass. A card is gone once it falls well below the window.
  const float bottom = static_cast<float>(kScreenHeight) + kCardHeight;
  std::erase_if(cards_, [bottom](const FlyingCard& card) {
    return card.position.y > bottom;
  });
  std::erase_if(confetti_, [](const Confetti& c) { return c.life <= 0.0f; });
}

void Celebration::Burst(Vector2 at, int count) {
  std::uniform_real_distribution<float> angle(0.0f,
                                              2.0f * std::numbers::pi_v<float>);
  std::uniform_real_distribution<float> speed(150.0f, 520.0f);
  std::uniform_real_distribution<float> spin(-720.0f, 720.0f);
  std::uniform_real_distribution<float> life(1.0f, 2.2f);
  std::uniform_real_distribution<float> hue(0.0f, 360.0f);
  for (int i = 0; i < count; ++i) {
    const float a = angle(rng_);
    const float s = speed(rng_);
    Confetti c;
    c.position = at;
    c.velocity = {std::cos(a) * s, std::sin(a) * s - 250.0f};
    c.spin = spin(rng_);
    c.life = life(rng_);
    // `ColorFromHSV` picks a colour by hue angle, so every scrap gets a
    // bright, fully saturated colour.
    c.color = ColorFromHSV(hue(rng_), 0.75f, 1.0f);
    confetti_.push_back(c);
  }
}

}  // namespace solitaire
