#include "renderer.h"

#include <raylib.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

#include "screen.h"
#include "theme.h"

/// @brief Everything that belongs to the Solitaire game.
namespace solitaire {

/// @brief Drawing helpers internal to `renderer.cpp`.
///
/// They are free functions rather than private members because they need no
/// Renderer state; keeping them out of the header also means changing one
/// never forces other files to recompile.
namespace {

/// @brief Number of segments used to draw each rounded corner.
constexpr int kCornerSegments = 8;

/// @brief How high a card looks off the table, which sets its shadow.
enum class Lift : std::uint8_t {
  /// @brief Lying on the table.
  Resting,
  /// @brief Sliding to a new pile.
  Flying,
  /// @brief Held by the mouse.
  Held,
};

/// @brief One card to draw, with the pile and position it belongs to.
struct CardSpot {
  /// @brief The card.
  Card card;
  /// @brief Pile it is in.
  PileId pile;
  /// @brief Position in that pile.
  int index = 0;
};

/// @brief Returns `color` with its alpha scaled by `factor`.
/// @param color Source colour.
/// @param factor `0.0` (transparent) to `1.0` (unchanged).
/// @return Faded copy (the parameter is a copy, so editing it is safe).
Color WithAlpha(Color color, float factor) {
  color.a = static_cast<unsigned char>(static_cast<float>(color.a) *
                                       std::clamp(factor, 0.0f, 1.0f));
  return color;
}

/// @brief Grows `rect` by `amount` on every side (negative shrinks it).
/// @param rect Rectangle to grow.
/// @param amount Pixels to add to each edge.
/// @return The larger rectangle, sharing the same centre.
Rectangle Outset(Rectangle rect, float amount) {
  return {rect.x - amount, rect.y - amount, rect.width + 2.0f * amount,
          rect.height + 2.0f * amount};
}

/// @brief Returns the centre point of `rect`.
/// @param rect Any rectangle.
/// @return Its centre.
Vector2 Centre(Rectangle rect) {
  return {rect.x + rect.width / 2.0f, rect.y + rect.height / 2.0f};
}

/// @brief Draws `text` centred on `centre` with raylib's default font.
/// @param text Null-terminated string (raylib is a C API, so it takes
///        `const char*` rather than `std::string`).
/// @param centre Point to centre the text on.
/// @param size Font size in pixels.
/// @param color Text colour.
void DrawTextCentred(const char* text, Vector2 centre, int size, Color color) {
  const int width = MeasureText(text, size);
  DrawText(text, static_cast<int>(centre.x) - width / 2,
           static_cast<int>(centre.y) - size / 2, size, color);
}

/// @brief Formats seconds as `m:ss`.
/// @param seconds Time to format.
/// @return Pointer into raylib's `TextFormat` buffer. It is only good until
///         the next few `TextFormat` calls, so use it straight away.
const char* FormatTime(float seconds) {
  const int whole = static_cast<int>(seconds);
  return TextFormat("%d:%02d", whole / 60, whole % 60);
}

/// @brief Draws a stand-in card when an image failed to load: a plain
/// rounded card with its rank and suit written on it.
/// @param dest Card bounds.
/// @param card The card.
/// @param face True to show the face, false for a back.
void DrawFallbackCard(Rectangle dest, const Card& card, bool face) {
  DrawRectangleRounded(dest, kCardRoundness, kCornerSegments,
                       face ? RAYWHITE : Color{40, 70, 150, 255});
  DrawRectangleRoundedLinesEx(dest, kCardRoundness, kCornerSegments, 1.0f,
                              Color{0, 0, 0, 80});
  if (!face || dest.width < 20.0f) return;
  const Color ink = IsRed(card.suit) ? Color{200, 30, 40, 255} : BLACK;
  DrawText(TextFormat("%s %s", RankName(card.rank), SuitName(card.suit)),
           static_cast<int>(dest.x + 8.0f), static_cast<int>(dest.y + 8.0f), 16,
           ink);
}

/// @brief Draws one card with its shadow.
/// @param textures Card images.
/// @param card The card.
/// @param position Top-left corner.
/// @param width_scale `0..1` horizontal squeeze while the card flips.
/// @param face True to show the face, false for the back.
/// @param back Card back design.
/// @param lift How high the card is, which sets the shadow.
void DrawCard(const CardTextures& textures, const Card& card, Vector2 position,
              float width_scale, bool face, int back, Lift lift) {
  const float width = kCardWidth * width_scale;
  // Squeeze about the card's centre line, so a flip turns in place.
  const Rectangle dest{position.x + (kCardWidth - width) / 2.0f, position.y,
                       width, kCardHeight};
  if (width < 1.0f) return;

  // Higher cards cast a softer, wider, further shadow.
  const float offset =
      lift == Lift::Held ? 9.0f : (lift == Lift::Flying ? 5.0f : 1.5f);
  const float alpha = lift == Lift::Resting ? 0.28f : 0.22f;
  DrawRectangleRounded(
      {dest.x + offset * 0.4f, dest.y + offset, dest.width, dest.height},
      kCardRoundness, kCornerSegments, WithAlpha(kShadow, alpha));
  if (lift != Lift::Resting) {
    DrawRectangleRounded(
        Outset(
            {dest.x + offset * 0.4f, dest.y + offset, dest.width, dest.height},
            3.0f),
        kCardRoundness, kCornerSegments, WithAlpha(kShadow, 0.08f));
  }

  const Texture2D& texture = face ? textures.Front(card) : textures.Back(back);
  if (texture.id == 0) {
    DrawFallbackCard(dest, card, face);
    return;
  }
  // `DrawTexturePro` stretches the source rectangle (the whole image) onto
  // `dest`. Origin {0, 0} and rotation 0 mean "no rotation".
  DrawTexturePro(texture,
                 {0.0f, 0.0f, static_cast<float>(texture.width),
                  static_cast<float>(texture.height)},
                 dest, {0.0f, 0.0f}, 0.0f, WHITE);
  // A faint edge keeps white cards apart where they overlap.
  DrawRectangleRoundedLinesEx(dest, kCardRoundness, kCornerSegments, 1.0f,
                              Color{0, 0, 0, 38});
}

/// @brief Draws the felt: a dark green table lit from the middle.
void DrawTable() {
  ClearBackground(kFeltDark);
  // `DrawCircleGradient` fades from the inner colour at the centre to the
  // outer colour at the rim, which looks like a lamp over the table.
  DrawCircleGradient(kScreenWidth / 2, kScreenHeight / 2 + 60, 900.0f,
                     kFeltLight, kFeltDark);
}

/// @brief Draws the marks where piles go: outlines for every pile, a ring
/// on an empty stock that can be turned over, and an "A" on empty
/// foundations.
/// @param game The table.
void DrawSlots(const Klondike& game) {
  auto slot = [](PileId pile) {
    const Rectangle rect = SlotRect(pile);
    DrawRectangleRounded(rect, kCardRoundness, kCornerSegments,
                         Color{0, 0, 0, 40});
    DrawRectangleRoundedLinesEx(rect, kCardRoundness, kCornerSegments, 2.0f,
                                kSlot);
    return rect;
  };
  const Rectangle stock = slot({PileKind::Stock, 0});
  if (game.position().stock.empty()) {
    const Vector2 c = Centre(stock);
    // A circle means "click to turn the waste over"; an empty one means
    // there is nothing left to draw.
    const bool can_recycle = !game.position().waste.empty();
    DrawRing(c, 26.0f, 32.0f, 0.0f, 360.0f, 40,
             can_recycle ? kSlot : WithAlpha(kSlot, 0.4f));
    if (can_recycle) {
      // raylib wants triangle corners in counter-clockwise order on screen.
      DrawTriangle({c.x + 22.0f, c.y - 12.0f}, {c.x + 30.0f, c.y + 2.0f},
                   {c.x + 38.0f, c.y - 12.0f}, kSlot);
    }
  }
  for (int f = 0; f < kFoundationCount; ++f) {
    const Rectangle rect = slot({PileKind::Foundation, f});
    DrawTextCentred("A", Centre(rect), 56, kSlot);
  }
  for (int c = 0; c < kTableauCount; ++c) {
    const Rectangle rect = slot({PileKind::Tableau, c});
    DrawTextCentred("K", Centre(rect), 56, WithAlpha(kSlot, 0.6f));
  }
}

/// @brief Collects every card on the table in drawing order: stock, waste,
/// foundations, then each column from the bottom card up.
/// @param game The table.
/// @return Every card with its pile and position.
std::vector<CardSpot> CardsInDrawOrder(const Klondike& game) {
  std::vector<CardSpot> spots;
  spots.reserve(kDeckSize);
  auto add = [&](PileId pile) {
    const std::vector<Card>& cards = game.Pile(pile);
    for (std::size_t i = 0; i < cards.size(); ++i) {
      spots.push_back({cards[i], pile, static_cast<int>(i)});
    }
  };
  add({PileKind::Stock, 0});
  add({PileKind::Waste, 0});
  for (int f = 0; f < kFoundationCount; ++f) add({PileKind::Foundation, f});
  for (int c = 0; c < kTableauCount; ++c) add({PileKind::Tableau, c});
  return spots;
}

/// @brief Returns true if a card is part of the run being dragged.
/// @param spot The card and where it is.
/// @param view UI state.
/// @return True for a held card.
bool IsHeld(const CardSpot& spot, const FrameView& view) {
  return view.dragging && view.dragging->pile == spot.pile &&
         spot.index >= view.dragging->index;
}

/// @brief Returns the rectangle around a run of cards in one pile, from
/// card `index` to the top.
/// @param game The table.
/// @param pile Pile the run is in.
/// @param index First card of the run.
/// @return Bounds in pixels.
Rectangle RunRect(const Klondike& game, PileId pile, int index) {
  const int top = static_cast<int>(game.Pile(pile).size()) - 1;
  if (top < 0) return SlotRect(pile);
  const Rectangle first = CardRect(game, pile, std::min(index, top));
  const Rectangle last = CardRect(game, pile, top);
  return {first.x, first.y, last.x + last.width - first.x,
          last.y + last.height - first.y};
}

/// @brief Draws a rounded glowing outline.
/// @param rect Area to outline.
/// @param color Outline colour.
/// @param alpha Opacity.
void Glow(Rectangle rect, Color color, float alpha) {
  DrawRectangleRoundedLinesEx(Outset(rect, 4.0f), kCardRoundness,
                              kCornerSegments, 4.0f, WithAlpha(color, alpha));
  DrawRectangleRoundedLinesEx(Outset(rect, 8.0f), kCardRoundness,
                              kCornerSegments, 3.0f,
                              WithAlpha(color, alpha * 0.35f));
}

/// @brief Draws the hint: the cards to move and where they go, or the
/// stock if the hint is to draw.
/// @param game The table.
/// @param view UI state.
void DrawHint(const Klondike& game, const FrameView& view) {
  if (!view.hint) return;
  const float age = view.now - view.hint_start;
  const float pulse = 0.55f + 0.45f * std::sin(age * 9.0f);
  if (view.hint->draw) {
    Glow(SlotRect({PileKind::Stock, 0}), kHint, pulse);
    return;
  }
  const Move& move = *view.hint->move;
  Glow(RunRect(game, move.from, move.index), kHint, pulse);
  const int size = static_cast<int>(game.Pile(move.to).size());
  Glow(size == 0 ? SlotRect(move.to) : CardRect(game, move.to, size - 1), kHint,
       pulse * 0.8f);
}

/// @brief Draws a button with a hover highlight.
/// @param button Which button.
/// @param label Button text.
/// @param view UI state (for the mouse).
/// @param active True to draw it highlighted (a selected tab).
/// @param enabled False to draw it dimmed.
void DrawButton(Button button, const char* label, const FrameView& view,
                bool active, bool enabled) {
  const Rectangle rect = ButtonRect(button);
  const bool hover = enabled && CheckCollisionPointRec(view.mouse, rect);
  const Color fill = active ? kButtonActive : (hover ? kButtonHover : kButton);
  DrawRectangleRounded(rect, 0.3f, kCornerSegments, fill);
  const Color text =
      active ? kButtonActiveText : (enabled ? kText : WithAlpha(kText, 0.35f));
  DrawTextCentred(label, Centre(rect), 18, text);
}

/// @brief Draws the top bar: buttons on the left, score, moves, and time on
/// the right, and the record for the draw mode under it.
/// @param game The table.
/// @param textures Card images (for the card-back preview).
/// @param view UI state.
void DrawBar(const Klondike& game, const CardTextures& textures,
             const FrameView& view) {
  DrawRectangleRounded(BarRect(), 0.25f, kCornerSegments, kBar);
  DrawButton(Button::NewGame, "NEW GAME", view, false, true);
  DrawButton(Button::Undo, "UNDO", view, false, game.CanUndo() && !view.won);
  DrawButton(Button::Hint, "HINT", view, false, !view.won);
  DrawButton(Button::DrawOne, "DRAW 1", view, game.draw_mode() == DrawMode::One,
             true);
  DrawButton(Button::DrawThree, "DRAW 3", view,
             game.draw_mode() == DrawMode::Three, true);

  // The card-back button shows a small picture of the current back.
  DrawButton(Button::CardBack, "", view, false, true);
  const Rectangle back_button = ButtonRect(Button::CardBack);
  const Texture2D& back = textures.Back(view.card_back);
  if (back.id != 0) {
    const float h = back_button.height - 10.0f;
    const float w = h / 1.4f;
    DrawTexturePro(back,
                   {0.0f, 0.0f, static_cast<float>(back.width),
                    static_cast<float>(back.height)},
                   {back_button.x + 10.0f, back_button.y + 5.0f, w, h},
                   {0.0f, 0.0f}, 0.0f, WHITE);
  }
  DrawText("BACK", static_cast<int>(back_button.x + 34.0f),
           static_cast<int>(back_button.y + 11.0f), 18, kText);
  DrawButton(Button::Sound, view.muted ? "SOUND OFF" : "SOUND ON", view, false,
             true);

  // `constexpr std::array` of C strings: labels known at compile time.
  constexpr std::array<const char*, 3> kLabels{"SCORE", "MOVES", "TIME"};
  for (int i = 0; i < 3; ++i) {
    const Rectangle rect = CounterRect(i);
    DrawRectangleRounded(rect, 0.3f, kCornerSegments, Color{0, 0, 0, 70});
    DrawText(kLabels[static_cast<std::size_t>(i)],
             static_cast<int>(rect.x + 10.0f), static_cast<int>(rect.y + 6.0f),
             12, kTextMuted);
    // `TextFormat` reuses a few internal buffers, so each value is
    // formatted right before it is drawn rather than all up front.
    const char* value = i == 0   ? TextFormat("%d", game.score())
                        : i == 1 ? TextFormat("%d", game.moves())
                                 : FormatTime(view.seconds);
    DrawText(value, static_cast<int>(rect.x + 10.0f),
             static_cast<int>(rect.y + 20.0f), 22, kText);
  }

  const ModeRecord& r = view.record;
  const char* record =
      r.wins > 0 ? TextFormat("%s   won %d of %d   best %s   streak %d",
                              DrawModeName(game.draw_mode()), r.wins, r.games,
                              FormatTime(r.best_time), r.streak)
                 : TextFormat("%s   won %d of %d",
                              DrawModeName(game.draw_mode()), r.wins, r.games);
  const int width = MeasureText(record, 14);
  const Rectangle bar = BarRect();
  DrawText(record, static_cast<int>(bar.x + bar.width) - width - 8,
           static_cast<int>(bar.y + bar.height + 5.0f), 14, kTextMuted);
}

/// @brief Draws the win celebration: tumbling cards and confetti.
/// @param celebration The celebration.
/// @param textures Card images.
/// @param now Animation clock.
void DrawCelebration(const Celebration& celebration,
                     const CardTextures& textures, float now) {
  for (const FlyingCard& flying : celebration.cards()) {
    if (now < flying.launch) continue;
    const Texture2D& texture = textures.Front(flying.card);
    if (texture.id == 0) continue;
    // With an origin at the card's centre, `DrawTexturePro` rotates the
    // card about its middle, and `dest.x/y` give that middle's position.
    DrawTexturePro(
        texture,
        {0.0f, 0.0f, static_cast<float>(texture.width),
         static_cast<float>(texture.height)},
        {flying.position.x, flying.position.y, kCardWidth, kCardHeight},
        {kCardWidth / 2.0f, kCardHeight / 2.0f}, flying.angle, WHITE);
  }
  for (const Confetti& c : celebration.confetti()) {
    DrawRectanglePro({c.position.x, c.position.y, 9.0f, 5.0f}, {4.5f, 2.5f},
                     c.angle, WithAlpha(c.color, std::min(1.0f, c.life)));
  }
}

/// @brief Draws the win panel once the celebration has had a moment.
/// @param game The table.
/// @param view UI state.
void DrawWinPanel(const Klondike& game, const FrameView& view) {
  const float since = view.now - view.won_at;
  if (!view.won || since < 1.4f) return;
  const float fade = std::clamp((since - 1.4f) / 0.4f, 0.0f, 1.0f);
  const Rectangle panel{static_cast<float>(kScreenWidth) / 2.0f - 270.0f,
                        static_cast<float>(kScreenHeight) / 2.0f - 170.0f,
                        540.0f, 340.0f};
  DrawRectangleRounded(Outset(panel, 4.0f), 0.12f, kCornerSegments,
                       WithAlpha(kGold, 0.6f * fade));
  DrawRectangleRounded(panel, 0.12f, kCornerSegments,
                       WithAlpha(Color{6, 36, 22, 245}, fade));
  const float cx = panel.x + panel.width / 2.0f;
  DrawTextCentred("YOU WIN!", {cx, panel.y + 56.0f}, 60,
                  WithAlpha(kGold, fade));
  DrawTextCentred(
      TextFormat("Time %s     Moves %d     Score %d", FormatTime(view.seconds),
                 game.moves(), game.score()),
      {cx, panel.y + 126.0f}, 22, WithAlpha(kText, fade));
  if (view.new_best) {
    const float pulse = 0.75f + 0.25f * std::sin(view.now * 6.0f);
    DrawTextCentred("NEW BEST TIME!", {cx, panel.y + 172.0f}, 28,
                    WithAlpha(kGold, fade * pulse));
  } else {
    DrawTextCentred(
        TextFormat("Best time %s", FormatTime(view.record.best_time)),
        {cx, panel.y + 172.0f}, 22, WithAlpha(kTextMuted, fade));
  }
  DrawTextCentred(TextFormat("Won %d of %d     Streak %d     Best streak %d",
                             view.record.wins, view.record.games,
                             view.record.streak, view.record.best_streak),
                  {cx, panel.y + 214.0f}, 18, WithAlpha(kTextMuted, fade));
  DrawTextCentred("Click or press N to deal again", {cx, panel.y + 280.0f}, 20,
                  WithAlpha(kText, fade));
}

/// @brief Draws a message pill at the bottom when no move is left.
/// @param view UI state.
void DrawStuck(const FrameView& view) {
  if (!view.stuck || view.won) return;
  const char* text = "No moves left. Press U to undo or N for a new game.";
  const float width = static_cast<float>(MeasureText(text, 20)) + 48.0f;
  const Rectangle pill{(static_cast<float>(kScreenWidth) - width) / 2.0f,
                       static_cast<float>(kScreenHeight) - 64.0f, width, 44.0f};
  DrawRectangleRounded(pill, 1.0f, kCornerSegments, Color{0, 0, 0, 190});
  DrawTextCentred(text, Centre(pill), 20, kText);
}

}  // namespace

void Renderer::Draw(const Klondike& game, const CardAnimator& animator,
                    const Celebration& celebration,
                    const CardTextures& textures, const FrameView& view) const {
  DrawTable();
  DrawSlots(game);

  // Three passes give the right overlap: resting cards first, then cards
  // sliding between piles (so they pass over the table), then the cards in
  // the player's hand on top of everything.
  const std::vector<CardSpot> spots = CardsInDrawOrder(game);
  auto draw_spot = [&](const CardSpot& spot, Lift lift) {
    DrawCard(textures, spot.card, animator.Position(spot.card),
             animator.FlipWidth(spot.card, view.now),
             animator.ShowsFace(spot.card, view.now), view.card_back, lift);
  };
  for (const CardSpot& spot : spots) {
    if (!IsHeld(spot, view) && !animator.InFlight(spot.card, view.now) &&
        !celebration.Launched(spot.card, view.now)) {
      draw_spot(spot, Lift::Resting);
    }
  }
  for (const CardSpot& spot : spots) {
    if (!IsHeld(spot, view) && animator.InFlight(spot.card, view.now) &&
        !celebration.Launched(spot.card, view.now)) {
      draw_spot(spot, Lift::Flying);
    }
  }

  DrawHint(game, view);
  if (view.drop_target) {
    const int size = static_cast<int>(game.Pile(*view.drop_target).size());
    Glow(size == 0 ? SlotRect(*view.drop_target)
                   : CardRect(game, *view.drop_target, size - 1),
         kDropTarget, 0.9f);
  }
  for (const CardSpot& spot : spots) {
    if (IsHeld(spot, view)) draw_spot(spot, Lift::Held);
  }

  DrawCelebration(celebration, textures, view.now);
  DrawBar(game, textures, view);
  DrawStuck(view);
  DrawWinPanel(game, view);
}

}  // namespace solitaire
