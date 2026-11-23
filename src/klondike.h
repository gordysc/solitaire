#ifndef SOLITAIRE_KLONDIKE_H_
#define SOLITAIRE_KLONDIKE_H_

#include <array>
#include <cstdint>
#include <optional>
#include <vector>

#include "card.h"

/// @brief Everything that belongs to the Solitaire game.
namespace solitaire {

/// @brief How many cards one click on the stock turns over.
enum class DrawMode : std::uint8_t {
  /// @brief One card at a time: the easier game.
  One = 0,
  /// @brief Three cards at a time, and only the top one can be played.
  Three = 1,
};

/// @brief Number of DrawMode values, for sizing per-mode arrays.
inline constexpr int kDrawModeCount = 2;
/// @brief Number of foundation piles (one per suit).
inline constexpr int kFoundationCount = 4;
/// @brief Number of tableau columns.
inline constexpr int kTableauCount = 7;

/// @brief Returns the display name of `mode`, such as `"DRAW 3"`.
/// @param mode Mode to name.
/// @return Pointer to a static string literal (never free it).
const char* DrawModeName(DrawMode mode);

/// @brief Returns the key used for `mode` in the save file, such as
/// `"draw3"`.
/// @param mode Mode to name.
/// @return Pointer to a static string literal.
const char* DrawModeKey(DrawMode mode);

/// @brief The four kinds of pile on the table.
enum class PileKind : std::uint8_t {
  /// @brief The face-down pile that cards are drawn from.
  Stock,
  /// @brief The face-up pile that drawn cards land on.
  Waste,
  /// @brief The four piles built up by suit from Ace to King.
  Foundation,
  /// @brief The seven columns built down in alternating colours.
  Tableau,
};

/// @brief Names one pile on the table.
struct PileId {
  /// @brief Kind of pile.
  PileKind kind = PileKind::Stock;
  /// @brief Which foundation (0..3) or tableau column (0..6). Always 0 for
  /// the stock and the waste.
  int index = 0;

  /// @brief Defaulted equality compares `kind` and `index`.
  bool operator==(const PileId&) const = default;
};

/// @brief One move of one or more face-up cards from pile to pile.
struct Move {
  /// @brief Pile the cards leave.
  PileId from;
  /// @brief Position in `from` of the first (lowest) card that moves. Every
  /// card above it moves with it.
  int index = 0;
  /// @brief Pile the cards land on.
  PileId to;

  /// @brief Defaulted equality compares every member.
  bool operator==(const Move&) const = default;
};

/// @brief A suggestion for the player.
struct Hint {
  /// @brief The move to make, or `std::nullopt` if the hint is "draw".
  std::optional<Move> move;
  /// @brief True if the best thing to do is to click the stock.
  bool draw = false;
};

/// @brief Every card on the table. In each pile the *last* element of the
/// vector is the top card, because adding and removing at the back of a
/// `std::vector` is fast and never moves the other elements.
struct Position {
  /// @brief Face-down draw pile.
  std::vector<Card> stock;
  /// @brief Face-up discard pile.
  std::vector<Card> waste;
  /// @brief Foundations, built Ace up to King by suit.
  std::array<std::vector<Card>, kFoundationCount> foundations;
  /// @brief Tableau columns. Face-down cards sit under face-up ones.
  std::array<std::vector<Card>, kTableauCount> tableau;

  /// @brief Defaulted equality compares every pile, card by card.
  bool operator==(const Position&) const = default;
};

/// @brief The rules of Klondike, the classic solitaire: dealing, drawing,
/// moving, scoring, undo, hints, and auto-complete.
///
/// It has no graphics. The Game asks it what is legal, tells it what the
/// player did, and draws whatever `position()` says.
///
/// Scoring follows the standard rules: +5 for a waste card played to the
/// tableau, +10 for any card played to a foundation, +5 for each tableau
/// card turned face up, -15 for a card taken back off a foundation, and a
/// penalty for going through the stock again (-100 each time in Draw 1;
/// in Draw 3 the first three passes are free, then -20 each). The score
/// never drops below zero.
class Klondike {
 public:
  /// @brief Creates an empty table. Call `Deal()` to start a game.
  Klondike() = default;

  /// @brief Shuffles a full deck and deals a new game.
  ///
  /// Column `i` gets `i + 1` cards with only the top one face up; the other
  /// 24 cards go to the stock.
  /// @param mode One or three cards per draw.
  /// @param seed 32-bit PRNG seed; the same seed always deals the same game.
  void Deal(DrawMode mode, std::uint32_t seed);

  /// @brief Replaces the table with an exact position, for tests.
  /// Resets the score, move count, and undo history.
  /// @param position Cards to place.
  /// @param mode One or three cards per draw.
  void LoadPosition(const Position& position, DrawMode mode);

  /// @brief Clicks the stock: turns over one or three cards onto the waste,
  /// or, if the stock is empty, turns the whole waste back over into a new
  /// stock.
  /// @return True if anything happened (false when both piles are empty).
  bool Draw();

  /// @brief Checks whether a move is legal.
  /// @param move Move to check.
  /// @return True if `ApplyMove(move)` would succeed.
  bool CanMove(const Move& move) const;

  /// @brief Makes a move if it is legal. If it uncovers a face-down tableau
  /// card, that card is turned face up as part of the same move.
  /// @param move Move to make.
  /// @return True if the move was made.
  bool ApplyMove(const Move& move);

  /// @brief Finds where a card should go when the player just clicks it:
  /// a foundation if possible, otherwise the first tableau column that
  /// takes it.
  /// @param from Pile the card is in.
  /// @param index Position of the card in that pile.
  /// @return A legal move, or `std::nullopt` if the card cannot go anywhere
  ///         useful.
  std::optional<Move> FindAutoMove(PileId from, int index) const;

  /// @brief Finds a good move to suggest, or suggests drawing.
  /// @return A hint, or `std::nullopt` if there is nothing left to try.
  std::optional<Hint> FindHint() const;

  /// @brief Returns true when the game can finish itself: the stock and the
  /// waste are empty and every tableau card is face up.
  /// @return True if every remaining card can go straight to a foundation.
  bool CanAutoComplete() const;

  /// @brief Returns the next foundation move for auto-complete: the
  /// lowest-ranked top card that can go up.
  /// @return The move, or `std::nullopt` if no card can go up.
  std::optional<Move> NextFoundationMove() const;

  /// @brief Takes back the last draw or move.
  /// @return True if there was something to take back.
  bool Undo();

  /// @brief Returns true if `Undo()` would do something.
  /// @return True when the history is not empty.
  bool CanUndo() const { return !history_.empty(); }

  /// @brief Returns true when all 52 cards are on the foundations.
  /// @return True for a won game.
  bool IsWon() const;

  /// @brief Returns the cards in one pile.
  /// @param pile Pile to read.
  /// @return The pile, bottom card first. An invalid pile gives an empty
  ///         vector. The reference stays valid until the next change.
  const std::vector<Card>& Pile(PileId pile) const;

  /// @brief The whole table.
  const Position& position() const { return position_; }
  /// @brief One or three cards per draw.
  DrawMode draw_mode() const { return mode_; }
  /// @brief Current score.
  int score() const { return score_; }
  /// @brief Draws and moves made (undo takes them back).
  int moves() const { return moves_; }
  /// @brief Number of times the waste was turned back into the stock.
  int recycles() const { return recycles_; }

 private:
  /// @brief Everything `Undo()` needs to restore one earlier state.
  struct Snapshot {
    /// @brief The table.
    Position position;
    /// @brief The score.
    int score = 0;
    /// @brief The move count.
    int moves = 0;
    /// @brief The recycle count.
    int recycles = 0;
  };

  /// @brief Saves the current state so the next change can be undone.
  void PushHistory();

  /// @brief Adds `points` to the score, never letting it drop below zero.
  /// @param points Points to add (negative for a penalty).
  void AddScore(int points);

  /// @brief Returns a writable reference to a pile.
  /// @param pile Pile to get (must be valid).
  /// @return Reference into `position_`.
  std::vector<Card>& MutablePile(PileId pile);

  /// @brief Every card on the table.
  Position position_;
  /// @brief One or three cards per draw.
  DrawMode mode_ = DrawMode::One;
  /// @brief Current score.
  int score_ = 0;
  /// @brief Draws and moves made.
  int moves_ = 0;
  /// @brief Times the waste was turned back into the stock.
  int recycles_ = 0;
  /// @brief Earlier states, oldest first, for `Undo()`.
  std::vector<Snapshot> history_;
};

}  // namespace solitaire

#endif  // SOLITAIRE_KLONDIKE_H_
