#include "game.h"

#include <raylib.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <vector>

/// @brief Everything that belongs to the Solitaire game.
namespace solitaire {

/// @brief Timing constants and helpers internal to `game.cpp`.
namespace {

/// @brief Mouse travel in pixels before a press becomes a drag.
constexpr float kDragThreshold = 5.0f;
/// @brief Seconds between two cards being dealt.
constexpr float kDealInterval = 0.035f;
/// @brief Seconds between two auto-complete moves.
constexpr float kAutoStepSeconds = 0.09f;
/// @brief Seconds a hint stays on screen.
constexpr float kHintSeconds = 3.0f;
/// @brief Seconds after the win before the cards start to fly.
constexpr float kCelebrationDelay = 0.6f;

/// @brief Returns the top-left corner of a rectangle.
/// @param rect Any rectangle.
/// @return Its top-left corner.
Vector2 Corner(Rectangle rect) { return {rect.x, rect.y}; }

}  // namespace

Game::Game() : save_path_(DefaultSavePath()), save_(LoadSaveData(save_path_)) {
  // Mix wall-clock time into the seed counter so every launch deals fresh
  // games. `time_since_epoch().count()` is the raw tick count.
  const auto now_ticks = static_cast<std::uint64_t>(
      std::chrono::steady_clock::now().time_since_epoch().count());
  seed_counter_ = static_cast<std::uint32_t>(now_ticks ^ (now_ticks >> 32U));
  audio_.set_muted(save_.muted);
  NewGame();
}

Game::~Game() {
  RecordAbandonedGame();
  PersistSaveData();
}

void Game::Update(float dt) {
  // Clamp so a long stall (window drag, breakpoint) can't add a huge jump.
  dt = std::clamp(dt, 0.0f, 0.1f);
  now_ += dt;
  if (clock_running_) clock_ += dt;

  HandleKeyboard();
  HandleMouse();

  if (!won_ && !drag_ && game_.CanAutoComplete() && now_ >= next_auto_step_) {
    StepAutoComplete();
  }
  if (hint_ && now_ - hint_start_ > kHintSeconds) hint_.reset();

  SyncAnimator(dt);
  celebration_.Update(dt, now_);
}

void Game::Draw() const {
  FrameView view;
  view.now = now_;
  view.mouse = GetMousePosition();
  view.card_back = save_.card_back;
  view.muted = audio_.muted();
  view.seconds = clock_;
  view.record = save_.records[static_cast<std::size_t>(game_.draw_mode())];
  view.hint = hint_;
  view.hint_start = hint_start_;
  if (drag_ && drag_->moved) view.dragging = PileHit{drag_->pile, drag_->index};
  if (drop_) view.drop_target = drop_->to;
  view.won = won_;
  view.won_at = won_at_;
  view.new_best = new_best_;
  view.stuck = stuck_;
  renderer_.Draw(game_, animator_, celebration_, textures_, view);
}

void Game::NewGame() {
  RecordAbandonedGame();
  game_.Deal(save_.draw_mode, NextSeed());
  clock_ = 0.0f;
  clock_running_ = false;
  won_ = false;
  new_best_ = false;
  result_recorded_ = false;
  stuck_ = false;
  hint_.reset();
  drag_.reset();
  drop_.reset();
  celebration_.Clear();

  // Dealing animation: every card starts face down on the stock. Tableau
  // cards leave one at a time in the order a person deals them (left to
  // right, one row at a time), and the top cards turn over as they leave.
  animator_.Reset();
  const Vector2 stock = Corner(SlotRect({PileKind::Stock, 0}));
  for (const Card& card : game_.position().stock) {
    animator_.Hold(card, stock, false, now_);
  }
  int dealt = 0;
  for (int row = 0; row < kTableauCount; ++row) {
    for (int column = row; column < kTableauCount; ++column) {
      const Card& card =
          game_.position().tableau[static_cast<std::size_t>(column)]
                                  [static_cast<std::size_t>(row)];
      animator_.Hold(card, stock, false,
                     now_ + 0.1f + kDealInterval * static_cast<float>(dealt));
      ++dealt;
    }
  }
  audio_.Play(SoundEffect::Deal);
}

void Game::SetDrawMode(DrawMode mode) {
  if (mode == game_.draw_mode()) return;
  // Record the old game while `game_` still says which mode it was.
  RecordAbandonedGame();
  save_.draw_mode = mode;
  PersistSaveData();
  NewGame();
}

void Game::HandleMouse() {
  const Vector2 mouse = GetMousePosition();

  if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
    for (int i = 0; i < kButtonCount; ++i) {
      const auto button = static_cast<Button>(i);
      if (CheckCollisionPointRec(mouse, ButtonRect(button))) {
        PressButton(button);
        return;
      }
    }
    if (won_) {
      // Give the celebration a moment so a stray click can't skip it.
      if (now_ - won_at_ > 1.0f) NewGame();
      return;
    }
    if (const auto hit = HitTest(game_, mouse)) {
      if (hit->pile.kind == PileKind::Stock) {
        DrawFromStock();
      } else if (hit->index >= 0) {
        const Card& card =
            game_.Pile(hit->pile)[static_cast<std::size_t>(hit->index)];
        if (card.face_up) {
          const Vector2 corner = Corner(CardRect(game_, hit->pile, hit->index));
          drag_ = Drag{hit->pile,
                       hit->index,
                       {mouse.x - corner.x, mouse.y - corner.y},
                       mouse,
                       false};
        }
      }
    }
  }

  if (drag_ && IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
    if (!drag_->moved &&
        std::hypot(mouse.x - drag_->press.x, mouse.y - drag_->press.y) >
            kDragThreshold) {
      drag_->moved = true;
      audio_.Play(SoundEffect::Pick);
    }
    if (drag_->moved) {
      const Rectangle held{mouse.x - drag_->grab.x, mouse.y - drag_->grab.y,
                           kCardWidth, kCardHeight};
      drop_ = FindDrop(game_, drag_->pile, drag_->index, held);
    }
  }

  if (drag_ && IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
    // Copy the drag out and clear it first, so the cards are no longer
    // "held" when the move below sends them flying to their new pile.
    const Drag drag = *drag_;
    const std::optional<Move> drop = drop_;
    drag_.reset();
    drop_.reset();
    if (drag.moved) {
      if (drop) {
        MakeMove(*drop);
      } else {
        // Not a legal spot: the animator slides the cards back home.
        audio_.Play(SoundEffect::Invalid);
      }
    } else if (const auto move = game_.FindAutoMove(drag.pile, drag.index)) {
      // A plain click sends the card wherever it fits best.
      MakeMove(*move);
    } else {
      audio_.Play(SoundEffect::Invalid);
    }
  }
}

void Game::HandleKeyboard() {
  if (IsKeyPressed(KEY_N) || IsKeyPressed(KEY_F2)) NewGame();
  if (IsKeyPressed(KEY_U) || IsKeyPressed(KEY_Z) ||
      IsKeyPressed(KEY_BACKSPACE)) {
    UndoMove();
  }
  if (IsKeyPressed(KEY_H)) ShowHint();
  if (IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_D)) DrawFromStock();
  if (IsKeyPressed(KEY_ONE)) SetDrawMode(DrawMode::One);
  if (IsKeyPressed(KEY_THREE)) SetDrawMode(DrawMode::Three);
  if (IsKeyPressed(KEY_B)) PressButton(Button::CardBack);
  if (IsKeyPressed(KEY_M)) PressButton(Button::Sound);
}

void Game::PressButton(Button button) {
  switch (button) {
    case Button::NewGame:
      NewGame();
      break;
    case Button::Undo:
      UndoMove();
      break;
    case Button::Hint:
      ShowHint();
      break;
    case Button::DrawOne:
      SetDrawMode(DrawMode::One);
      break;
    case Button::DrawThree:
      SetDrawMode(DrawMode::Three);
      break;
    case Button::CardBack:
      save_.card_back = (save_.card_back + 1) % kCardBackCount;
      PersistSaveData();
      break;
    case Button::Sound:
      audio_.set_muted(!audio_.muted());
      save_.muted = audio_.muted();
      PersistSaveData();
      break;
  }
}

void Game::DrawFromStock() {
  if (won_ || drag_) return;
  const bool recycling = game_.position().stock.empty();
  if (!game_.Draw()) return;
  StartClock();
  audio_.Play(recycling ? SoundEffect::Undo : SoundEffect::Draw);
  AfterChange();
}

void Game::MakeMove(const Move& move) {
  const std::vector<Card>& source = game_.Pile(move.from);
  if (move.index < 0 || move.index >= static_cast<int>(source.size())) return;
  // Read what is needed for the sounds *before* the move: afterwards these
  // cards have moved, and `source` may be shorter.
  const int rank = source[static_cast<std::size_t>(move.index)].rank;
  const bool turns_card_over =
      move.from.kind == PileKind::Tableau && move.index > 0 &&
      !source[static_cast<std::size_t>(move.index - 1)].face_up;
  if (!game_.ApplyMove(move)) {
    audio_.Play(SoundEffect::Invalid);
    return;
  }
  StartClock();
  if (move.to.kind == PileKind::Foundation) {
    audio_.PlayFoundation(rank);
  } else {
    audio_.Play(SoundEffect::Place);
  }
  if (turns_card_over) audio_.Play(SoundEffect::Flip);
  AfterChange();
}

void Game::UndoMove() {
  if (won_ || drag_) return;
  if (game_.Undo()) {
    audio_.Play(SoundEffect::Undo);
    AfterChange();
  }
}

void Game::ShowHint() {
  if (won_) return;
  hint_ = game_.FindHint();
  hint_start_ = now_;
  if (hint_) {
    audio_.Play(SoundEffect::Hint);
  } else {
    stuck_ = true;
    audio_.Play(SoundEffect::Invalid);
  }
}

void Game::AfterChange() {
  hint_.reset();
  if (game_.IsWon()) {
    Win();
    return;
  }
  // `FindHint()` suggests drawing while the stock or waste has cards, so it
  // finds nothing only when the player truly has no move at all.
  stuck_ = !game_.FindHint().has_value();
}

void Game::Win() {
  if (won_) return;
  won_ = true;
  won_at_ = now_;
  clock_running_ = false;
  stuck_ = false;
  if (!result_recorded_) {
    result_recorded_ = true;
    new_best_ = RecordResult(save_, game_.draw_mode(),
                             {true, clock_, game_.moves(), game_.score()});
    PersistSaveData();
  }
  audio_.Play(SoundEffect::Win);

  // Cards leave the foundations from the top: every King, then every Queen,
  // and so on, taking turns between the four piles.
  std::vector<FlyingCard> flying;
  for (int rank = kKing; rank >= kAce; --rank) {
    for (int f = 0; f < kFoundationCount; ++f) {
      const PileId pile{PileKind::Foundation, f};
      const std::vector<Card>& cards = game_.Pile(pile);
      if (static_cast<int>(cards.size()) < rank) continue;
      const Rectangle rect = CardRect(game_, pile, rank - 1);
      FlyingCard card;
      card.card = cards[static_cast<std::size_t>(rank - 1)];
      card.position = {rect.x + rect.width / 2.0f, rect.y + rect.height / 2.0f};
      flying.push_back(card);
    }
  }
  celebration_.Start(flying, now_ + kCelebrationDelay);
}

void Game::StepAutoComplete() {
  if (const auto move = game_.NextFoundationMove()) {
    MakeMove(*move);
    next_auto_step_ = now_ + kAutoStepSeconds;
  }
}

void Game::SyncAnimator(float dt) {
  const Vector2 mouse = GetMousePosition();
  // Tells the animator where every card of one pile belongs. Held cards
  // follow the mouse exactly, keeping their spacing in the column.
  auto sync_pile = [&](PileId pile) {
    const std::vector<Card>& cards = game_.Pile(pile);
    for (int i = 0; i < static_cast<int>(cards.size()); ++i) {
      Vector2 target = Corner(CardRect(game_, pile, i));
      const bool held =
          drag_ && drag_->moved && drag_->pile == pile && i >= drag_->index;
      if (held) {
        const Vector2 first = Corner(CardRect(game_, pile, drag_->index));
        target = {mouse.x - drag_->grab.x + (target.x - first.x),
                  mouse.y - drag_->grab.y + (target.y - first.y)};
      }
      animator_.Follow(cards[static_cast<std::size_t>(i)], target, held, now_,
                       dt);
    }
  };
  sync_pile({PileKind::Stock, 0});
  sync_pile({PileKind::Waste, 0});
  for (int f = 0; f < kFoundationCount; ++f) {
    sync_pile({PileKind::Foundation, f});
  }
  for (int c = 0; c < kTableauCount; ++c) sync_pile({PileKind::Tableau, c});
}

void Game::StartClock() {
  if (!won_) clock_running_ = true;
}

void Game::RecordAbandonedGame() {
  if (result_recorded_ || won_ || game_.moves() == 0) return;
  result_recorded_ = true;
  RecordResult(save_, game_.draw_mode(),
               {false, clock_, game_.moves(), game_.score()});
  PersistSaveData();
}

void Game::PersistSaveData() { WriteSaveData(save_path_, save_); }

std::uint32_t Game::NextSeed() {
  // Multiply by a large odd constant (Knuth's multiplicative hash) so
  // consecutive counters give very different seeds. Unsigned overflow wraps
  // around by definition, so this is well-defined.
  seed_counter_ += 1U;
  return seed_counter_ * 2654435761U;
}

}  // namespace solitaire
