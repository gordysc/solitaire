#include "card.h"

#include <array>

/// @brief Everything that belongs to the Solitaire game.
namespace solitaire {

const char* RankName(int rank) {
  // A `constexpr std::array` of string literals is a lookup table built at
  // compile time. Index 0 is unused so that `rank` can index it directly.
  constexpr std::array<const char*, kRankCount + 1> kNames{
      "?", "ace", "2", "3",  "4",    "5",     "6",
      "7", "8",   "9", "10", "jack", "queen", "king"};
  if (rank < kAce || rank > kKing) return "?";
  return kNames[static_cast<std::size_t>(rank)];
}

const char* SuitName(Suit suit) {
  // A `switch` over an `enum class` with no `default:` lets the compiler warn
  // (-Wswitch, part of -Wall) if a new Suit is added but not handled here.
  switch (suit) {
    case Suit::Clubs:
      return "clubs";
    case Suit::Diamonds:
      return "diamonds";
    case Suit::Hearts:
      return "hearts";
    case Suit::Spades:
      return "spades";
  }
  return "?";
}

}  // namespace solitaire
