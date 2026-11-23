#include "layout.h"

#include <algorithm>
#include <array>

#include "screen.h"

/// @brief Everything that belongs to the Solitaire game.
namespace solitaire {

/// @brief Layout constants internal to `layout.cpp`.
namespace {

/// @brief Space between two columns.
constexpr float kColumnGap = 20.0f;
/// @brief Left edge of column 0, which centres the seven columns.
constexpr float kLeft =
    (static_cast<float>(kScreenWidth) -
     (kTableauCount * kCardWidth + (kTableauCount - 1) * kColumnGap)) /
    2.0f;
/// @brief Top edge of the stock, waste, and foundations.
constexpr float kTopRowY = 92.0f;
/// @brief Top edge of the tableau columns.
constexpr float kTableauY = kTopRowY + kCardHeight + 28.0f;
/// @brief Vertical step between face-down tableau cards.
constexpr float kFaceDownStep = 14.0f;
/// @brief Vertical step between face-up tableau cards, before squeezing.
constexpr float kFaceUpStep = 32.0f;
/// @brief Smallest step a squeezed column may use.
constexpr float kMinFaceUpStep = 12.0f;
/// @brief Space kept free under the longest column.
constexpr float kBottomMargin = 16.0f;
/// @brief Horizontal step between the fanned Draw 3 waste cards.
constexpr float kWasteStep = 26.0f;
/// @brief Cards per one-pixel step of the "thick pile" look on the stock.
constexpr int kCardsPerDepthStep = 6;

/// @brief Returns the left edge of column `column` of the 7-column grid.
/// @param column Column number (0..6).
/// @return Pixels.
float ColumnX(int column) {
  return kLeft + static_cast<float>(column) * (kCardWidth + kColumnGap);
}

/// @brief Returns the rectangle of the top card of a pile, or its slot
/// when it is empty. Used as the drop area of the pile.
/// @param game Game to read the pile from.
/// @param pile Pile to locate.
/// @return Bounds in pixels.
Rectangle TopRect(const Klondike& game, PileId pile) {
  const int size = static_cast<int>(game.Pile(pile).size());
  return size == 0 ? SlotRect(pile) : CardRect(game, pile, size - 1);
}

}  // namespace

Rectangle SlotRect(PileId pile) {
  switch (pile.kind) {
    case PileKind::Stock:
      return {ColumnX(0), kTopRowY, kCardWidth, kCardHeight};
    case PileKind::Waste:
      return {ColumnX(1), kTopRowY, kCardWidth, kCardHeight};
    case PileKind::Foundation:
      return {ColumnX(3 + pile.index), kTopRowY, kCardWidth, kCardHeight};
    case PileKind::Tableau:
      return {ColumnX(pile.index), kTableauY, kCardWidth, kCardHeight};
  }
  return {};
}

float FaceUpStep(const Klondike& game, int column) {
  const std::vector<Card>& cards = game.Pile({PileKind::Tableau, column});
  // `std::ranges::count_if` counts the elements the lambda accepts.
  const auto face_down = static_cast<float>(std::ranges::count_if(
      cards, [](const Card& card) { return !card.face_up; }));
  const float face_up = static_cast<float>(cards.size()) - face_down;
  if (face_up <= 1.0f) return kFaceUpStep;
  const float room = static_cast<float>(kScreenHeight) - kBottomMargin -
                     kTableauY - kCardHeight - face_down * kFaceDownStep;
  return std::clamp(room / (face_up - 1.0f), kMinFaceUpStep, kFaceUpStep);
}

Rectangle CardRect(const Klondike& game, PileId pile, int index) {
  Rectangle rect = SlotRect(pile);
  const std::vector<Card>& cards = game.Pile(pile);
  switch (pile.kind) {
    case PileKind::Stock:
    case PileKind::Foundation: {
      // Lift each group of cards a little up and to the left, so a big
      // pile looks thick.
      const float depth = static_cast<float>(index / kCardsPerDepthStep);
      rect.x -= depth;
      rect.y -= depth;
      break;
    }
    case PileKind::Waste: {
      const int size = static_cast<int>(cards.size());
      const int fanned =
          game.draw_mode() == DrawMode::Three ? std::min(3, size) : 1;
      const int first_fanned = size - fanned;
      if (index > first_fanned) {
        rect.x += static_cast<float>(index - first_fanned) * kWasteStep;
      }
      break;
    }
    case PileKind::Tableau: {
      const float up_step = FaceUpStep(game, pile.index);
      const int limit = std::min(index, static_cast<int>(cards.size()));
      for (int i = 0; i < limit; ++i) {
        rect.y += cards[static_cast<std::size_t>(i)].face_up ? up_step
                                                             : kFaceDownStep;
      }
      break;
    }
  }
  return rect;
}

std::optional<PileHit> HitTest(const Klondike& game, Vector2 point) {
  for (int c = 0; c < kTableauCount; ++c) {
    const PileId pile{PileKind::Tableau, c};
    const int size = static_cast<int>(game.Pile(pile).size());
    if (size == 0) {
      if (CheckCollisionPointRec(point, SlotRect(pile))) {
        return PileHit{pile, -1};
      }
      continue;
    }
    // Check from the top card down: the top card is drawn last, so it is
    // the one the player sees (and means) where cards overlap.
    for (int i = size - 1; i >= 0; --i) {
      if (CheckCollisionPointRec(point, CardRect(game, pile, i))) {
        return PileHit{pile, i};
      }
    }
  }
  for (int f = 0; f < kFoundationCount; ++f) {
    const PileId pile{PileKind::Foundation, f};
    if (CheckCollisionPointRec(point, TopRect(game, pile))) {
      return PileHit{pile, static_cast<int>(game.Pile(pile).size()) - 1};
    }
  }
  const PileId waste{PileKind::Waste, 0};
  if (!game.Pile(waste).empty() &&
      CheckCollisionPointRec(point, TopRect(game, waste))) {
    return PileHit{waste, static_cast<int>(game.Pile(waste).size()) - 1};
  }
  const PileId stock{PileKind::Stock, 0};
  if (CheckCollisionPointRec(point, SlotRect(stock))) {
    return PileHit{stock, -1};
  }
  return std::nullopt;
}

std::optional<Move> FindDrop(const Klondike& game, PileId from, int index,
                             Rectangle dragged) {
  std::optional<Move> best;
  float best_area = 0.0f;
  // Tries one pile: keeps it if the move is legal and overlaps more than
  // the best so far. `[&]` captures the locals above by reference.
  auto consider = [&](PileId pile) {
    const Move move{from, index, pile};
    if (!game.CanMove(move)) return;
    // `GetCollisionRec` returns the overlapping part of two rectangles.
    const Rectangle overlap = GetCollisionRec(dragged, TopRect(game, pile));
    const float area = overlap.width * overlap.height;
    if (area > best_area) {
      best_area = area;
      best = move;
    }
  };
  for (int f = 0; f < kFoundationCount; ++f) {
    consider({PileKind::Foundation, f});
  }
  for (int c = 0; c < kTableauCount; ++c) consider({PileKind::Tableau, c});
  return best;
}

Rectangle BarRect() {
  return {12.0f, 10.0f, static_cast<float>(kScreenWidth) - 24.0f, 60.0f};
}

Rectangle ButtonRect(Button button) {
  // `constexpr std::array`: the button widths, fixed at compile time.
  constexpr std::array<float, kButtonCount> kWidths{112.0f, 82.0f, 76.0f, 88.0f,
                                                    88.0f,  84.0f, 100.0f};
  constexpr float kGap = 8.0f;
  const Rectangle bar = BarRect();
  float x = bar.x + 12.0f;
  const auto target = static_cast<std::size_t>(button);
  for (std::size_t i = 0; i < target; ++i) x += kWidths[i] + kGap;
  // The draw-mode pair is set slightly apart from the other buttons.
  if (button >= Button::DrawOne) x += 10.0f;
  if (button >= Button::CardBack) x += 10.0f;
  return {x, bar.y + 10.0f, kWidths[target], 40.0f};
}

Rectangle CounterRect(int slot) {
  const Rectangle bar = BarRect();
  constexpr float kWidth = 128.0f;
  return {bar.x + bar.width - 12.0f - static_cast<float>(3 - slot) * kWidth,
          bar.y + 8.0f, kWidth - 8.0f, 44.0f};
}

}  // namespace solitaire
