#ifndef SOLITAIRE_GAME_H_
#define SOLITAIRE_GAME_H_

#include <raylib.h>

#include <cstdint>
#include <filesystem>
#include <optional>

#include "audio.h"
#include "effects.h"
#include "klondike.h"
#include "layout.h"
#include "renderer.h"
#include "storage.h"
#include "textures.h"

/// @brief Everything that belongs to the Solitaire game.
namespace solitaire {

/// @brief Top-level coordinator: reads input, drives the Klondike rules,
/// keeps the clock, runs animations and sounds, records results, and hands
/// everything to the Renderer.
///
/// `main()` constructs one `Game` inside an explicit RAII scope block so its
/// destructor (which saves progress and frees textures and sounds) runs
/// before `CloseAudioDevice()` and `CloseWindow()`.
class Game {
 public:
  /// @brief Loads card images, sounds, and saved records, then deals.
  Game();

  /// @brief Records an unfinished game and saves on clean shutdown.
  ~Game();

  /// @brief Non-copyable: there is only ever one game, and a copy would free
  /// the same textures and sounds twice.
  Game(const Game&) = delete;
  /// @brief Non-copy-assignable for the same reason.
  Game& operator=(const Game&) = delete;

  /// @brief Handles input and advances animations by one frame.
  /// @param dt Seconds elapsed since the previous frame.
  void Update(float dt);

  /// @brief Renders the current frame between raylib's `BeginDrawing()` and
  /// `EndDrawing()`. Marked `const` so drawing can never mutate game state.
  void Draw() const;

 private:
  /// @brief A run of cards the player is holding with the mouse.
  struct Drag {
    /// @brief Pile the run comes from.
    PileId pile;
    /// @brief Position of the run's first card in that pile.
    int index = 0;
    /// @brief Mouse position relative to the first card's top-left corner,
    /// so the card doesn't jump to put its corner under the pointer.
    Vector2 grab{0.0f, 0.0f};
    /// @brief Where the mouse button went down.
    Vector2 press{0.0f, 0.0f};
    /// @brief True once the mouse has moved far enough to count as a drag
    /// rather than a click.
    bool moved = false;
  };

  /// @brief Shuffles and deals a new game with a dealing animation.
  void NewGame();

  /// @brief Switches between Draw 1 and Draw 3 (dealing a new game).
  /// @param mode Mode to switch to.
  void SetDrawMode(DrawMode mode);

  /// @brief Handles clicks and drags on the table and the top bar.
  void HandleMouse();

  /// @brief Handles hotkeys.
  void HandleKeyboard();

  /// @brief Handles a click on a top-bar button.
  /// @param button The button.
  void PressButton(Button button);

  /// @brief Clicks the stock (draws, or turns the waste over).
  void DrawFromStock();

  /// @brief Makes a move and plays its sounds.
  /// @param move The move (checked for legality inside).
  void MakeMove(const Move& move);

  /// @brief Takes back the last move.
  void UndoMove();

  /// @brief Shows a hint, or the "no moves" message if there is none.
  void ShowHint();

  /// @brief Runs after every change to the table: clears the hint, checks
  /// for a win, and checks whether the player is stuck.
  void AfterChange();

  /// @brief Celebrates and records a win.
  void Win();

  /// @brief Moves one card to a foundation while auto-complete is running.
  void StepAutoComplete();

  /// @brief Tells the animator where every card should be this frame.
  /// @param dt Seconds since the last frame.
  void SyncAnimator(float dt);

  /// @brief Starts the game clock on the first draw or move.
  void StartClock();

  /// @brief Records a game the player is leaving before winning.
  void RecordAbandonedGame();

  /// @brief Writes `save_` to `save_path_` so records persist.
  void PersistSaveData();

  /// @brief Generates a fresh seed for a new deal.
  /// @return 32-bit PRNG seed.
  std::uint32_t NextSeed();

  /// @brief Filesystem path where records are stored.
  std::filesystem::path save_path_;
  /// @brief Loaded settings and records.
  SaveData save_{};
  /// @brief Card images.
  CardTextures textures_;
  /// @brief Synthesised sounds.
  Audio audio_;
  /// @brief The rules and the cards on the table.
  Klondike game_;
  /// @brief Where each card is drawn.
  CardAnimator animator_;
  /// @brief The win celebration.
  Celebration celebration_;
  /// @brief Stateless drawing subsystem.
  Renderer renderer_;

  /// @brief Animation clock in seconds.
  float now_ = 0.0f;
  /// @brief Game clock in seconds.
  float clock_ = 0.0f;
  /// @brief True from the first move until the game is won.
  bool clock_running_ = false;
  /// @brief Animation clock time of the next auto-complete step.
  float next_auto_step_ = 0.0f;

  /// @brief True once the game is won.
  bool won_ = false;
  /// @brief Animation clock time of the win.
  float won_at_ = 0.0f;
  /// @brief True if the win set a new best time.
  bool new_best_ = false;
  /// @brief True once this game's result has been recorded.
  bool result_recorded_ = false;
  /// @brief True if no move is left and nothing can be drawn.
  bool stuck_ = false;

  /// @brief The hint being shown, if any.
  std::optional<Hint> hint_;
  /// @brief Animation clock time when the hint appeared.
  float hint_start_ = 0.0f;
  /// @brief The run being dragged, if any.
  std::optional<Drag> drag_;
  /// @brief Where the dragged run would land if dropped now.
  std::optional<Move> drop_;

  /// @brief Monotonic counter mixed into seeds so each deal is different.
  std::uint32_t seed_counter_ = 1;
};

}  // namespace solitaire

#endif  // SOLITAIRE_GAME_H_
