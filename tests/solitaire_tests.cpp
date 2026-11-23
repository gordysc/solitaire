#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <system_error>
#include <vector>

#include "card.h"
#include "klondike.h"
#include "storage.h"

/// @brief Minimal self-contained unit test suite for `solitaire_core`.
namespace {

// `using` declarations bring a few names into this namespace so the tests
// can write `Klondike` instead of `solitaire::Klondike`.
using solitaire::Card;
using solitaire::DrawMode;
using solitaire::Klondike;
using solitaire::Move;
using solitaire::PileId;
using solitaire::PileKind;
using solitaire::Position;
using solitaire::Suit;

/// @brief Total number of assertions that failed during this test run.
int g_failures = 0;

/// @brief Checks a boolean condition and prints a diagnostic message on
/// failure.
/// @param condition Expression expected to be true.
/// @param message Human-readable failure description.
void Expect(bool condition, const char* message) {
  if (!condition) {
    std::cerr << "FAIL: " << message << "\n";
    ++g_failures;
  }
}

/// @brief Makes a card.
/// @param rank Rank from 1 (Ace) to 13 (King).
/// @param suit Suit.
/// @param face_up True for a face-up card (the default).
/// @return The card.
Card C(int rank, Suit suit, bool face_up = true) {
  return {suit, rank, face_up};
}

/// @brief Shorthand pile ids for the tests.
constexpr PileId kWaste{PileKind::Waste, 0};
/// @brief Shorthand for foundation `i`.
/// @param i Foundation number.
/// @return The PileId.
constexpr PileId F(int i) { return {PileKind::Foundation, i}; }
/// @brief Shorthand for tableau column `i`.
/// @param i Column number.
/// @return The PileId.
constexpr PileId T(int i) { return {PileKind::Tableau, i}; }

/// @brief Checks every rule the table must always obey: 52 different cards,
/// a face-down stock, a face-up waste, foundations built up by suit, and
/// tableau columns with face-down cards under a legal face-up run.
/// @param game Game to check.
/// @return True if every rule holds.
bool TableIsConsistent(const Klondike& game) {
  const Position& p = game.position();
  std::array<int, solitaire::kDeckSize> seen{};
  int total = 0;
  // A lambda that counts each card it is given. It captures `seen` and
  // `total` by reference so it updates the locals above.
  auto count = [&](const std::vector<Card>& pile) {
    for (const Card& card : pile) {
      ++seen[static_cast<std::size_t>(solitaire::CardId(card))];
      ++total;
    }
  };
  count(p.stock);
  count(p.waste);
  for (const auto& f : p.foundations) count(f);
  for (const auto& t : p.tableau) count(t);
  if (total != solitaire::kDeckSize) return false;
  for (const int n : seen) {
    if (n != 1) return false;
  }
  for (const Card& card : p.stock) {
    if (card.face_up) return false;
  }
  for (const Card& card : p.waste) {
    if (!card.face_up) return false;
  }
  for (const auto& f : p.foundations) {
    for (std::size_t i = 0; i < f.size(); ++i) {
      if (f[i].rank != static_cast<int>(i) + 1 || f[i].suit != f[0].suit) {
        return false;
      }
    }
  }
  for (const auto& t : p.tableau) {
    bool face_up_seen = false;
    for (std::size_t i = 0; i < t.size(); ++i) {
      if (face_up_seen && !t[i].face_up) return false;
      if (t[i].face_up && face_up_seen) {
        const Card& below = t[i - 1];
        if (t[i].rank != below.rank - 1 ||
            solitaire::IsRed(t[i].suit) == solitaire::IsRed(below.suit)) {
          return false;
        }
      }
      face_up_seen = face_up_seen || t[i].face_up;
    }
    if (!t.empty() && !t.back().face_up) return false;
  }
  return true;
}

/// @brief Verifies the deal: 1..7 cards per column with the top face up,
/// 24 face-down cards in the stock, and the same game for the same seed.
void TestDeal() {
  for (std::uint32_t seed = 1; seed <= 50; ++seed) {
    Klondike game;
    game.Deal(DrawMode::One, seed);
    const Position& p = game.position();
    bool columns_ok = true;
    for (int c = 0; c < solitaire::kTableauCount; ++c) {
      const auto& column = p.tableau[static_cast<std::size_t>(c)];
      columns_ok = columns_ok &&
                   column.size() == static_cast<std::size_t>(c + 1) &&
                   column.back().face_up;
      for (std::size_t i = 0; i + 1 < column.size(); ++i) {
        columns_ok = columns_ok && !column[i].face_up;
      }
    }
    Expect(columns_ok, "Column i has i + 1 cards, only the top face up");
    Expect(p.stock.size() == 24 && p.waste.empty(),
           "24 cards go to the stock and the waste starts empty");
    Expect(TableIsConsistent(game), "A new deal uses all 52 cards once");
    Expect(game.score() == 0 && game.moves() == 0 && !game.CanUndo(),
           "A new deal starts at zero with no history");
  }
  Klondike a;
  Klondike b;
  Klondike c;
  a.Deal(DrawMode::One, 7);
  b.Deal(DrawMode::One, 7);
  c.Deal(DrawMode::One, 8);
  Expect(a.position() == b.position(), "The same seed deals the same game");
  Expect(!(a.position() == c.position()), "Different seeds deal differently");
}

/// @brief Verifies drawing one or three cards and turning the waste over.
void TestDraw() {
  Klondike game;
  game.Deal(DrawMode::One, 3);
  const Card first = game.position().stock.back();
  Expect(game.Draw(), "Drawing from a full stock works");
  Expect(
      game.position().waste.size() == 1 && game.position().stock.size() == 23,
      "Draw 1 moves one card");
  Expect(game.position().waste.back().face_up &&
             game.position().waste.back().rank == first.rank &&
             game.position().waste.back().suit == first.suit,
         "The top stock card lands face up on the waste");

  for (int i = 0; i < 23; ++i) game.Draw();
  Expect(game.position().stock.empty() && game.position().waste.size() == 24,
         "The whole stock can be drawn");
  Expect(game.Draw(), "Drawing from an empty stock turns the waste over");
  Expect(game.position().stock.size() == 24 && game.position().waste.empty(),
         "Every waste card goes back to the stock");
  Expect(game.recycles() == 1, "The pass through the stock is counted");
  game.Draw();
  Expect(game.position().waste.back().rank == first.rank &&
             game.position().waste.back().suit == first.suit,
         "After turning over, the first card drawn comes up first again");
  Expect(TableIsConsistent(game), "Drawing keeps the table consistent");

  Klondike three;
  three.Deal(DrawMode::Three, 3);
  const std::vector<Card> stock = three.position().stock;
  three.Draw();
  Expect(
      three.position().waste.size() == 3 && three.position().stock.size() == 21,
      "Draw 3 moves three cards");
  const Card third_from_top = stock[stock.size() - 3];
  Expect(three.position().waste.back().rank == third_from_top.rank &&
             three.position().waste.back().suit == third_from_top.suit,
         "In Draw 3 the third card turned over ends on top");

  Position empty;
  game.LoadPosition(empty, DrawMode::One);
  Expect(!game.Draw() && !game.CanUndo(),
         "Drawing with no stock and no waste does nothing");
}

/// @brief Verifies the foundation rules: Ace first, then up by suit.
void TestFoundationRules() {
  Position p;
  p.waste = {C(2, Suit::Spades), C(2, Suit::Hearts), C(1, Suit::Hearts)};
  Klondike game;
  game.LoadPosition(p, DrawMode::One);
  Expect(!game.CanMove({kWaste, 1, F(0)}), "Only the waste top can move");
  Expect(game.ApplyMove({kWaste, 2, F(0)}), "An Ace starts a foundation");
  Expect(game.score() == 10, "A card to a foundation scores 10");
  Expect(game.ApplyMove({kWaste, 1, F(0)}), "2 of hearts goes on the Ace");
  Expect(!game.CanMove({kWaste, 0, F(0)}), "2 of spades does not");
  Expect(!game.CanMove({kWaste, 0, F(1)}),
         "Only an Ace can start an empty foundation");
  Expect(!game.CanMove({F(0), 1, F(1)}),
         "Cards never move from foundation to foundation");
}

/// @brief Verifies the tableau rules: down by one in alternating colours,
/// and only Kings on an empty column.
void TestTableauRules() {
  Position p;
  p.tableau[0] = {C(13, Suit::Spades)};
  p.tableau[1] = {C(12, Suit::Hearts)};
  p.tableau[2] = {C(12, Suit::Clubs)};
  p.tableau[4] = {C(13, Suit::Diamonds)};
  p.tableau[5] = {C(5, Suit::Clubs, false), C(11, Suit::Hearts)};
  Klondike game;
  game.LoadPosition(p, DrawMode::One);
  Expect(game.CanMove({T(1), 0, T(0)}), "A red Queen goes on a black King");
  Expect(!game.CanMove({T(2), 0, T(0)}), "Same colours do not stack");
  Expect(!game.CanMove({T(0), 0, T(1)}), "Higher cards do not go on lower");
  Expect(game.CanMove({T(4), 0, T(3)}), "A King goes on an empty column");
  Expect(!game.CanMove({T(1), 0, T(3)}), "A Queen does not");
  Expect(!game.CanMove({T(5), 0, T(3)}), "Face-down cards never move");
  Expect(!game.CanMove({T(1), 0, T(1)}), "A pile cannot move onto itself");
  Expect(!game.CanMove({T(1), 0, {PileKind::Waste, 0}}),
         "Nothing moves onto the waste");
  Expect(!game.CanMove({T(1), 0, T(9)}), "Columns past 6 do not exist");
}

/// @brief Verifies moving a run and turning over the card it uncovers.
void TestRunMoveAndTurnOver() {
  Position p;
  p.tableau[0] = {C(2, Suit::Diamonds, false), C(8, Suit::Hearts),
                  C(7, Suit::Clubs), C(6, Suit::Diamonds)};
  p.tableau[1] = {C(9, Suit::Clubs)};
  p.tableau[2] = {C(8, Suit::Diamonds)};
  Klondike game;
  game.LoadPosition(p, DrawMode::One);
  Expect(game.ApplyMove({T(0), 2, T(2)}),
         "The 7 of clubs moves with the 6 on top of it");
  Expect(game.position().tableau[2].size() == 3, "Both cards moved");
  Expect(game.ApplyMove({T(0), 1, T(1)}), "The 8 of hearts moves to the 9");
  Expect(game.position().tableau[0].size() == 1 &&
             game.position().tableau[0].back().face_up,
         "The uncovered card turns face up");
  Expect(game.score() == 5, "Turning a card over scores 5");
  Expect(game.moves() == 2, "Each move is counted");
}

/// @brief Verifies that undo restores the exact earlier state.
void TestUndo() {
  Klondike game;
  game.Deal(DrawMode::Three, 11);
  const Position start = game.position();
  Expect(!game.Undo(), "Nothing to undo at the start");
  game.Draw();
  game.Draw();
  const auto hint = game.FindHint();
  if (hint && hint->move) game.ApplyMove(*hint->move);
  while (game.CanUndo()) game.Undo();
  Expect(game.position() == start, "Undo goes all the way back to the deal");
  Expect(game.score() == 0 && game.moves() == 0,
         "Undo also restores the score and move count");
}

/// @brief Verifies scoring penalties never push the score below zero.
void TestScorePenalties() {
  Position p;
  p.waste = {C(1, Suit::Spades)};
  p.tableau[0] = {C(13, Suit::Hearts), C(12, Suit::Spades), C(11, Suit::Hearts),
                  C(10, Suit::Spades), C(9, Suit::Hearts),  C(8, Suit::Spades),
                  C(7, Suit::Hearts),  C(6, Suit::Spades),  C(5, Suit::Hearts),
                  C(4, Suit::Spades),  C(3, Suit::Hearts)};
  p.foundations[1] = {C(1, Suit::Clubs), C(2, Suit::Clubs)};
  Klondike game;
  game.LoadPosition(p, DrawMode::One);
  game.ApplyMove({kWaste, 0, F(0)});
  Expect(game.score() == 10, "Waste to foundation scores 10");
  Expect(game.ApplyMove({F(1), 1, T(0)}),
         "A card can come back off a foundation");
  Expect(game.score() == 0, "Taking a card off a foundation costs 15, to 0");
}

/// @brief Verifies the one-click move picks a foundation first and avoids
/// pointless moves.
void TestAutoMove() {
  Position p;
  p.waste = {C(1, Suit::Clubs)};
  p.tableau[0] = {C(13, Suit::Hearts)};
  p.tableau[1] = {C(4, Suit::Clubs, false), C(13, Suit::Spades)};
  p.tableau[2] = {C(10, Suit::Clubs)};
  p.tableau[3] = {C(9, Suit::Diamonds)};
  Klondike game;
  game.LoadPosition(p, DrawMode::One);
  const auto ace = game.FindAutoMove(kWaste, 0);
  Expect(ace && ace->to.kind == PileKind::Foundation, "An Ace goes up");
  Expect(!game.FindAutoMove(T(0), 0),
         "A King that already starts a column has nowhere useful to go");
  const auto king = game.FindAutoMove(T(1), 1);
  Expect(king && king->to.kind == PileKind::Tableau &&
             game.position()
                 .tableau[static_cast<std::size_t>(king->to.index)]
                 .empty(),
         "A King on a face-down card goes to an empty column");
  const auto nine = game.FindAutoMove(T(3), 0);
  Expect(nine && nine->to == T(2), "The 9 of diamonds goes on the 10");
}

/// @brief Verifies hints, in order of usefulness.
void TestHints() {
  Position p;
  p.waste = {C(1, Suit::Hearts)};
  p.tableau[0] = {C(5, Suit::Clubs, false), C(7, Suit::Spades)};
  p.tableau[1] = {C(8, Suit::Hearts)};
  Klondike game;
  game.LoadPosition(p, DrawMode::One);
  auto hint = game.FindHint();
  Expect(hint && hint->move && hint->move->to.kind == PileKind::Foundation,
         "A card that can go up is the first hint");
  game.ApplyMove(*hint->move);
  hint = game.FindHint();
  Expect(hint && hint->move && *hint->move == Move{T(0), 1, T(1)},
         "Then a move that turns a card over");

  Position stuck;
  stuck.stock = {C(9, Suit::Clubs, false)};
  stuck.tableau[0] = {C(5, Suit::Hearts)};
  game.LoadPosition(stuck, DrawMode::One);
  hint = game.FindHint();
  Expect(hint && !hint->move && hint->draw,
         "With no move left, the hint is to draw");

  Position done;
  done.tableau[0] = {C(5, Suit::Hearts)};
  game.LoadPosition(done, DrawMode::One);
  Expect(!game.FindHint(), "With no move and no stock there is no hint");
}

/// @brief Verifies auto-complete finishes a game and detects the win.
void TestAutoCompleteAndWin() {
  Position p;
  for (int s = 0; s < solitaire::kSuitCount; ++s) {
    for (int rank = 1; rank <= 10; ++rank) {
      p.foundations[static_cast<std::size_t>(s)].push_back(
          C(rank, static_cast<Suit>(s)));
    }
  }
  p.tableau[0] = {C(13, Suit::Spades), C(12, Suit::Hearts),
                  C(11, Suit::Spades)};
  p.tableau[1] = {C(13, Suit::Hearts), C(12, Suit::Spades),
                  C(11, Suit::Hearts)};
  p.tableau[2] = {C(13, Suit::Clubs), C(12, Suit::Diamonds),
                  C(11, Suit::Clubs)};
  p.tableau[3] = {C(13, Suit::Diamonds), C(12, Suit::Clubs),
                  C(11, Suit::Diamonds)};
  Klondike game;
  game.LoadPosition(p, DrawMode::One);
  Expect(TableIsConsistent(game), "The near-won table is consistent");
  Expect(game.CanAutoComplete(), "Every card is face up: auto-complete works");
  int steps = 0;
  while (const auto move = game.NextFoundationMove()) {
    Expect(game.ApplyMove(*move), "Each auto-complete move is legal");
    if (++steps > 20) break;
  }
  Expect(steps == 12, "Twelve cards go up");
  Expect(game.IsWon(), "All 52 cards on the foundations is a win");
  Expect(!game.CanAutoComplete(), "A won game has nothing to complete");
}

/// @brief Plays many games by following the hints and checks the table
/// stays consistent after every step.
void TestPlayWithHints() {
  int wins = 0;
  for (std::uint32_t seed = 1; seed <= 60; ++seed) {
    Klondike game;
    game.Deal(seed % 2 == 0 ? DrawMode::One : DrawMode::Three, seed);
    for (int step = 0; step < 600 && !game.IsWon(); ++step) {
      if (game.CanAutoComplete()) {
        const auto move = game.NextFoundationMove();
        Expect(move && game.ApplyMove(*move), "Auto-complete never stalls");
        continue;
      }
      const auto hint = game.FindHint();
      if (!hint) break;
      if (hint->move) {
        Expect(game.ApplyMove(*hint->move), "Every hinted move is legal");
      } else {
        Expect(game.Draw(), "A draw hint can always draw");
      }
      if (!TableIsConsistent(game)) {
        Expect(false, "The table stays consistent during play");
        break;
      }
    }
    if (game.IsWon()) ++wins;
  }
  // The hints are a simple greedy player, so they win only some deals. One
  // win proves the whole path to a won game works.
  Expect(wins > 0, "Following the hints wins at least one of 60 deals");
}

/// @brief Verifies card names used for the image files.
void TestNames() {
  Expect(std::strcmp(solitaire::RankName(1), "ace") == 0 &&
             std::strcmp(solitaire::RankName(10), "10") == 0 &&
             std::strcmp(solitaire::RankName(13), "king") == 0,
         "RankName gives file-name ranks");
  Expect(std::strcmp(solitaire::RankName(0), "?") == 0, "Bad ranks give ?");
  Expect(std::strcmp(solitaire::SuitName(Suit::Hearts), "hearts") == 0,
         "SuitName gives file-name suits");
  Expect(solitaire::CardId(C(1, Suit::Clubs)) == 0 &&
             solitaire::CardId(C(13, Suit::Spades)) == 51,
         "CardId numbers the deck 0..51");
}

/// @brief Verifies records round-trip through a save file on disk.
void TestStorageRoundTrip() {
  const std::filesystem::path temp_path =
      std::filesystem::temp_directory_path() / "solitaire_test.cfg";

  solitaire::SaveData out;
  out.draw_mode = DrawMode::Three;
  out.card_back = 2;
  out.muted = true;
  Expect(!solitaire::RecordResult(out, DrawMode::Three, {false, 50.0f, 40, 0}),
         "A game given up sets no best");
  Expect(
      solitaire::RecordResult(out, DrawMode::Three, {true, 200.5f, 120, 600}),
      "The first win sets a best time");
  Expect(
      !solitaire::RecordResult(out, DrawMode::Three, {true, 300.0f, 100, 500}),
      "A slower win is not a best time");
  const auto& r = out.records[1];
  Expect(r.games == 3 && r.wins == 2 && r.streak == 2 && r.best_streak == 2,
         "Games, wins, and streaks are counted");
  Expect(r.best_moves == 100 && r.best_score == 600,
         "Fewest moves and highest score are kept separately");
  Expect(solitaire::WriteSaveData(temp_path, out), "WriteSaveData succeeds");

  const solitaire::SaveData in = solitaire::LoadSaveData(temp_path);
  Expect(in.draw_mode == DrawMode::Three, "draw_mode round-trips");
  Expect(in.card_back == 2 && in.muted, "card_back and muted round-trip");
  const auto& l = in.records[1];
  Expect(l.games == 3 && l.wins == 2 && l.streak == 2 && l.best_streak == 2 &&
             l.best_moves == 100 && l.best_score == 600 &&
             std::abs(l.best_time - 200.5f) < 0.01f,
         "Every draw3 record round-trips");
  Expect(in.records[0].games == 0, "Draw 1 records stay empty");

  // `std::error_code` overload: reports failure through `ec` instead of
  // throwing, so cleanup can never abort the test run.
  std::error_code ec;
  std::filesystem::remove(temp_path, ec);
}

}  // namespace

/// @brief Entry point for the CTest unit test binary.
/// @return 0 if all tests pass, non-zero otherwise.
int main() {
  TestDeal();
  TestDraw();
  TestFoundationRules();
  TestTableauRules();
  TestRunMoveAndTurnOver();
  TestUndo();
  TestScorePenalties();
  TestAutoMove();
  TestHints();
  TestAutoCompleteAndWin();
  TestPlayWithHints();
  TestNames();
  TestStorageRoundTrip();

  if (g_failures > 0) {
    std::cerr << g_failures << " test assertion(s) failed.\n";
    return 1;
  }
  std::cout << "All solitaire_core tests passed!\n";
  return 0;
}
