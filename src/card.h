#ifndef SOLITAIRE_CARD_H_
#define SOLITAIRE_CARD_H_

#include <cstdint>

/// @brief Everything that belongs to the Solitaire game.
namespace solitaire {

/// @brief The four suits, in the order of the foundation piles on screen.
///
/// `enum class` keeps the names scoped (`Suit::Hearts`) and stops them from
/// turning into `int`s by accident. `: std::uint8_t` makes each value one
/// byte instead of the default `int`.
enum class Suit : std::uint8_t {
  /// @brief Black clubs.
  Clubs = 0,
  /// @brief Red diamonds.
  Diamonds,
  /// @brief Red hearts.
  Hearts,
  /// @brief Black spades.
  Spades,
};

/// @brief Number of suits, for sizing per-suit arrays.
inline constexpr int kSuitCount = 4;
/// @brief Number of ranks per suit (Ace = 1 up to King = 13).
inline constexpr int kRankCount = 13;
/// @brief Number of cards in a deck (no jokers).
inline constexpr int kDeckSize = kSuitCount * kRankCount;
/// @brief Rank number of the Ace.
inline constexpr int kAce = 1;
/// @brief Rank number of the King.
inline constexpr int kKing = 13;

/// @brief One playing card.
struct Card {
  /// @brief Suit.
  Suit suit = Suit::Clubs;
  /// @brief Rank from 1 (Ace) to 13 (King). A plain `int` rather than an
  /// enum, because the rules do arithmetic on it ("one lower than").
  int rank = kAce;
  /// @brief True if the card shows its face.
  bool face_up = false;

  /// @brief Defaulted equality (C++20) compares every member.
  bool operator==(const Card&) const = default;
};

/// @brief Returns true for diamonds and hearts.
/// @param suit Suit to test.
/// @return True for a red suit.
constexpr bool IsRed(Suit suit) {
  return suit == Suit::Diamonds || suit == Suit::Hearts;
}

/// @brief Returns a number from 0 to 51 that names one card of the deck,
/// whether it is face up or not. Handy as an index into per-card arrays.
/// @param card Card to number.
/// @return `suit * 13 + (rank - 1)`.
constexpr int CardId(const Card& card) {
  return static_cast<int>(card.suit) * kRankCount + (card.rank - 1);
}

/// @brief Returns the lower-case name of `rank`, as used in the card image
/// file names: `"ace"`, `"2"` ... `"10"`, `"jack"`, `"queen"`, `"king"`.
/// @param rank Rank from 1 to 13.
/// @return Pointer to a static string literal (never free it).
const char* RankName(int rank);

/// @brief Returns the lower-case plural name of `suit`, such as `"hearts"`.
/// @param suit Suit to name.
/// @return Pointer to a static string literal.
const char* SuitName(Suit suit);

}  // namespace solitaire

#endif  // SOLITAIRE_CARD_H_
