#include "klondike.h"

#include <algorithm>
#include <random>
#include <utility>

/// @brief Everything that belongs to the Solitaire game.
namespace solitaire {

/// @brief Rule helpers internal to `klondike.cpp`.
namespace {

/// @brief Points for a waste card played to the tableau.
constexpr int kWasteToTableauPoints = 5;
/// @brief Points for any card played to a foundation.
constexpr int kToFoundationPoints = 10;
/// @brief Points for turning a tableau card face up.
constexpr int kTurnOverPoints = 5;
/// @brief Points for taking a card back off a foundation (a penalty).
constexpr int kFromFoundationPoints = -15;
/// @brief Penalty for each pass through the stock in Draw 1.
constexpr int kRecycleDrawOnePoints = -100;
/// @brief Penalty for each pass through the stock in Draw 3, after the free
/// ones.
constexpr int kRecycleDrawThreePoints = -20;
/// @brief Passes through the stock that are free in Draw 3.
constexpr int kFreeRecyclesDrawThree = 3;

/// @brief The empty pile that `Klondike::Pile()` returns for a bad PileId.
///
/// `Pile()` returns a reference, so it needs an object that outlives every
/// call. A namespace-scope constant lives for the whole program, so a
/// reference to it can never dangle.
const std::vector<Card> kEmptyPile;

/// @brief Returns true if `pile` names a pile that exists.
/// @param pile Pile to check.
/// @return False for an out-of-range foundation or column index.
bool IsValidPile(PileId pile) {
  switch (pile.kind) {
    case PileKind::Stock:
    case PileKind::Waste:
      return pile.index == 0;
    case PileKind::Foundation:
      return pile.index >= 0 && pile.index < kFoundationCount;
    case PileKind::Tableau:
      return pile.index >= 0 && pile.index < kTableauCount;
  }
  return false;
}

/// @brief Returns true if `card` may go on top of `foundation`: an Ace on an
/// empty pile, or the next rank up of the same suit.
/// @param foundation Foundation pile.
/// @param card Card to place.
/// @return True if legal.
bool FitsFoundation(const std::vector<Card>& foundation, const Card& card) {
  if (foundation.empty()) return card.rank == kAce;
  const Card& top = foundation.back();
  return top.suit == card.suit && card.rank == top.rank + 1;
}

/// @brief Returns true if `card` may go on top of `column`: a King on an
/// empty column, or one rank lower and the other colour.
/// @param column Tableau column.
/// @param card Lowest card of the run to place.
/// @return True if legal.
bool FitsTableau(const std::vector<Card>& column, const Card& card) {
  if (column.empty()) return card.rank == kKing;
  const Card& top = column.back();
  return top.face_up && IsRed(top.suit) != IsRed(card.suit) &&
         card.rank == top.rank - 1;
}

/// @brief Returns the position of the lowest face-up card in a column.
/// @param column Tableau column.
/// @return Index of the first face-up card, or `column.size()` if none.
int FirstFaceUp(const std::vector<Card>& column) {
  // `std::ranges::find_if` returns an iterator to the first element the
  // lambda accepts; subtracting `begin()` turns it into an index.
  const auto it = std::ranges::find_if(
      column, [](const Card& card) { return card.face_up; });
  return static_cast<int>(it - column.begin());
}

/// @brief Returns a foundation pile id.
/// @param index Foundation number (0..3).
/// @return The PileId.
constexpr PileId Foundation(int index) { return {PileKind::Foundation, index}; }

/// @brief Returns a tableau column id.
/// @param index Column number (0..6).
/// @return The PileId.
constexpr PileId Tableau(int index) { return {PileKind::Tableau, index}; }

/// @brief The waste pile id.
constexpr PileId kWaste{PileKind::Waste, 0};

}  // namespace

const char* DrawModeName(DrawMode mode) {
  switch (mode) {
    case DrawMode::One:
      return "DRAW 1";
    case DrawMode::Three:
      return "DRAW 3";
  }
  return "?";
}

const char* DrawModeKey(DrawMode mode) {
  switch (mode) {
    case DrawMode::One:
      return "draw1";
    case DrawMode::Three:
      return "draw3";
  }
  return "unknown";
}

void Klondike::Deal(DrawMode mode, std::uint32_t seed) {
  std::vector<Card> deck;
  deck.reserve(kDeckSize);
  for (int s = 0; s < kSuitCount; ++s) {
    for (int rank = kAce; rank <= kKing; ++rank) {
      deck.push_back({static_cast<Suit>(s), rank, false});
    }
  }
  // `std::mt19937` is a seeded random engine, so one seed always gives the
  // same shuffle. `std::ranges::shuffle` is a fair Fisher-Yates shuffle.
  std::mt19937 rng(seed);
  std::ranges::shuffle(deck, rng);

  Position position;
  // Deal like a person would: one card to each column from left to right,
  // then start again one column further right, until column i has i + 1.
  for (int row = 0; row < kTableauCount; ++row) {
    for (int column = row; column < kTableauCount; ++column) {
      position.tableau[static_cast<std::size_t>(column)].push_back(deck.back());
      deck.pop_back();
    }
  }
  for (std::vector<Card>& column : position.tableau) {
    column.back().face_up = true;
  }
  // `std::move` hands the remaining cards' buffer to the stock instead of
  // copying them. `deck` is left valid but empty-ish ("moved-from"); it is
  // not used again.
  position.stock = std::move(deck);
  LoadPosition(position, mode);
}

void Klondike::LoadPosition(const Position& position, DrawMode mode) {
  position_ = position;
  mode_ = mode;
  score_ = 0;
  moves_ = 0;
  recycles_ = 0;
  history_.clear();
}

bool Klondike::Draw() {
  std::vector<Card>& stock = position_.stock;
  std::vector<Card>& waste = position_.waste;
  if (stock.empty() && waste.empty()) return false;
  PushHistory();
  if (!stock.empty()) {
    const int count = mode_ == DrawMode::Three ? 3 : 1;
    for (int i = 0; i < count && !stock.empty(); ++i) {
      Card card = stock.back();
      stock.pop_back();
      card.face_up = true;
      waste.push_back(card);
    }
  } else {
    // Turn the whole waste over. Taking cards off the top of the waste and
    // putting each one on the stock reverses the order, so the first card
    // drawn ends up on top again, exactly like flipping the pile over.
    while (!waste.empty()) {
      Card card = waste.back();
      waste.pop_back();
      card.face_up = false;
      stock.push_back(card);
    }
    ++recycles_;
    if (mode_ == DrawMode::One) {
      AddScore(kRecycleDrawOnePoints);
    } else if (recycles_ > kFreeRecyclesDrawThree) {
      AddScore(kRecycleDrawThreePoints);
    }
  }
  ++moves_;
  return true;
}

bool Klondike::CanMove(const Move& move) const {
  if (!IsValidPile(move.from) || !IsValidPile(move.to)) return false;
  if (move.from == move.to) return false;
  // Cards only ever leave the stock by drawing, and only ever land on the
  // waste by drawing.
  if (move.from.kind == PileKind::Stock) return false;
  if (move.to.kind == PileKind::Stock || move.to.kind == PileKind::Waste) {
    return false;
  }
  if (move.from.kind == PileKind::Foundation &&
      move.to.kind == PileKind::Foundation) {
    return false;
  }

  const std::vector<Card>& source = Pile(move.from);
  const int size = static_cast<int>(source.size());
  if (move.index < 0 || move.index >= size) return false;
  const Card& moving = source[static_cast<std::size_t>(move.index)];
  if (!moving.face_up) return false;
  const int count = size - move.index;
  // Only tableau columns let you pick up a run of several cards. The face-up
  // cards of a column always form a legal run, because they only ever got
  // there by legal moves.
  if (move.from.kind != PileKind::Tableau && count != 1) return false;

  const std::vector<Card>& target = Pile(move.to);
  if (move.to.kind == PileKind::Foundation) {
    return count == 1 && FitsFoundation(target, moving);
  }
  return FitsTableau(target, moving);
}

bool Klondike::ApplyMove(const Move& move) {
  if (!CanMove(move)) return false;
  PushHistory();
  // Two references into two *different* vectors (CanMove rejected
  // `from == to`), so changing one never invalidates the other.
  std::vector<Card>& source = MutablePile(move.from);
  std::vector<Card>& target = MutablePile(move.to);
  const auto first = source.begin() + move.index;
  // Copy the run onto the target, then cut it off the source. Erasing
  // invalidates iterators at and after `first`, so `first` is not used
  // after the erase.
  target.insert(target.end(), first, source.end());
  source.erase(first, source.end());

  if (move.to.kind == PileKind::Foundation) {
    AddScore(kToFoundationPoints);
  } else if (move.from.kind == PileKind::Waste) {
    AddScore(kWasteToTableauPoints);
  } else if (move.from.kind == PileKind::Foundation) {
    AddScore(kFromFoundationPoints);
  }
  if (move.from.kind == PileKind::Tableau && !source.empty() &&
      !source.back().face_up) {
    source.back().face_up = true;
    AddScore(kTurnOverPoints);
  }
  ++moves_;
  return true;
}

std::optional<Move> Klondike::FindAutoMove(PileId from, int index) const {
  const std::vector<Card>& source = Pile(from);
  if (index < 0 || index >= static_cast<int>(source.size())) {
    return std::nullopt;
  }
  if (index == static_cast<int>(source.size()) - 1 &&
      from.kind != PileKind::Foundation) {
    for (int f = 0; f < kFoundationCount; ++f) {
      const Move move{from, index, Foundation(f)};
      if (CanMove(move)) return move;
    }
  }
  // An empty column is the last choice, and never for a run that already
  // starts a column: moving it would change nothing.
  std::optional<Move> empty_column;
  for (int c = 0; c < kTableauCount; ++c) {
    const Move move{from, index, Tableau(c)};
    if (!CanMove(move)) continue;
    if (!Pile(move.to).empty()) return move;
    const bool pointless = from.kind == PileKind::Tableau && index == 0;
    if (!pointless && !empty_column) empty_column = move;
  }
  return empty_column;
}

std::optional<Hint> Klondike::FindHint() const {
  const std::vector<Card>& waste = position_.waste;

  // 1. Anything that can go up to a foundation.
  if (!waste.empty()) {
    const int top = static_cast<int>(waste.size()) - 1;
    for (int f = 0; f < kFoundationCount; ++f) {
      const Move move{kWaste, top, Foundation(f)};
      if (CanMove(move)) return Hint{move};
    }
  }
  for (int c = 0; c < kTableauCount; ++c) {
    const std::vector<Card>& column = Pile(Tableau(c));
    if (column.empty()) continue;
    const int top = static_cast<int>(column.size()) - 1;
    for (int f = 0; f < kFoundationCount; ++f) {
      const Move move{Tableau(c), top, Foundation(f)};
      if (CanMove(move)) return Hint{move};
    }
  }

  // 2. Moving a whole face-up run off a face-down card turns that card over.
  for (int c = 0; c < kTableauCount; ++c) {
    const std::vector<Card>& column = Pile(Tableau(c));
    const int first = FirstFaceUp(column);
    if (first == 0 || first >= static_cast<int>(column.size())) continue;
    for (int d = 0; d < kTableauCount; ++d) {
      const Move move{Tableau(c), first, Tableau(d)};
      if (CanMove(move)) return Hint{move};
    }
  }

  // 3. Playing the waste card onto the tableau.
  if (!waste.empty()) {
    const int top = static_cast<int>(waste.size()) - 1;
    for (int d = 0; d < kTableauCount; ++d) {
      const Move move{kWaste, top, Tableau(d)};
      if (CanMove(move)) return Hint{move};
    }
  }

  // 4. Moving part of a run so the card under it can go to a foundation.
  for (int c = 0; c < kTableauCount; ++c) {
    const std::vector<Card>& column = Pile(Tableau(c));
    for (int i = FirstFaceUp(column) + 1; i < static_cast<int>(column.size());
         ++i) {
      const Card& under = column[static_cast<std::size_t>(i - 1)];
      // `std::ranges::any_of` is true if the lambda accepts any foundation.
      const bool under_can_go_up = std::ranges::any_of(
          position_.foundations, [&under](const std::vector<Card>& f) {
            return FitsFoundation(f, under);
          });
      if (!under_can_go_up) continue;
      for (int d = 0; d < kTableauCount; ++d) {
        const Move move{Tableau(c), i, Tableau(d)};
        if (CanMove(move)) return Hint{move};
      }
    }
  }

  // 5. Nothing to move: try the stock.
  if (!position_.stock.empty() || !waste.empty()) {
    return Hint{std::nullopt, true};
  }
  return std::nullopt;
}

bool Klondike::CanAutoComplete() const {
  if (!position_.stock.empty() || !position_.waste.empty() || IsWon()) {
    return false;
  }
  for (const std::vector<Card>& column : position_.tableau) {
    for (const Card& card : column) {
      if (!card.face_up) return false;
    }
  }
  return true;
}

std::optional<Move> Klondike::NextFoundationMove() const {
  std::optional<Move> best;
  int best_rank = kKing + 1;
  // Looks at one candidate pile: if its top card can go up and is lower
  // than the best so far, remember it. Captures `this`, `best`, and
  // `best_rank` by reference; it is only called inside this function.
  auto consider = [&](PileId pile) {
    const std::vector<Card>& cards = Pile(pile);
    if (cards.empty() || cards.back().rank >= best_rank) return;
    const int top = static_cast<int>(cards.size()) - 1;
    for (int f = 0; f < kFoundationCount; ++f) {
      const Move move{pile, top, Foundation(f)};
      if (CanMove(move)) {
        best = move;
        best_rank = cards.back().rank;
        return;
      }
    }
  };
  consider(kWaste);
  for (int c = 0; c < kTableauCount; ++c) consider(Tableau(c));
  return best;
}

bool Klondike::Undo() {
  if (history_.empty()) return false;
  // `std::move` takes the snapshot's piles instead of copying them; the
  // moved-from snapshot is popped straight after, so nobody sees it.
  Snapshot& last = history_.back();
  position_ = std::move(last.position);
  score_ = last.score;
  moves_ = last.moves;
  recycles_ = last.recycles;
  history_.pop_back();
  return true;
}

bool Klondike::IsWon() const {
  return std::ranges::all_of(
      position_.foundations,
      [](const std::vector<Card>& f) { return f.size() == kRankCount; });
}

const std::vector<Card>& Klondike::Pile(PileId pile) const {
  if (!IsValidPile(pile)) return kEmptyPile;
  switch (pile.kind) {
    case PileKind::Stock:
      return position_.stock;
    case PileKind::Waste:
      return position_.waste;
    case PileKind::Foundation:
      return position_.foundations[static_cast<std::size_t>(pile.index)];
    case PileKind::Tableau:
      return position_.tableau[static_cast<std::size_t>(pile.index)];
  }
  return kEmptyPile;
}

void Klondike::PushHistory() {
  // A full copy of the table is only 52 small cards, so storing whole
  // snapshots is simpler and safer than recording how to reverse each move.
  history_.push_back({position_, score_, moves_, recycles_});
}

void Klondike::AddScore(int points) { score_ = std::max(0, score_ + points); }

std::vector<Card>& Klondike::MutablePile(PileId pile) {
  // The usual trick to avoid writing `Pile()` twice: call the const version
  // through a const view of `*this`, then cast the const away. That is safe
  // because `*this` really is non-const here, and `pile` was already
  // checked, so this never returns `kEmptyPile` (which is truly const).
  return const_cast<std::vector<Card>&>(std::as_const(*this).Pile(pile));
}

}  // namespace solitaire
