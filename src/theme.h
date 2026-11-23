#ifndef SOLITAIRE_THEME_H_
#define SOLITAIRE_THEME_H_

#include <raylib.h>

/// @brief Everything that belongs to the Solitaire game.
namespace solitaire {

// raylib's `Color` is a plain C struct of four `unsigned char`s, which makes
// it a "literal type": it can be built at compile time, so each colour here
// is an `inline constexpr` constant with no runtime cost.

/// @brief Felt at the edges of the table.
inline constexpr Color kFeltDark{8, 58, 36, 255};
/// @brief Felt in the lit centre of the table.
inline constexpr Color kFeltLight{22, 112, 66, 255};
/// @brief Top bar behind the buttons and counters.
inline constexpr Color kBar{6, 40, 25, 220};
/// @brief Button fill.
inline constexpr Color kButton{18, 84, 52, 255};
/// @brief Button fill under the mouse pointer.
inline constexpr Color kButtonHover{30, 110, 70, 255};
/// @brief Selected tab and highlighted button fill.
inline constexpr Color kButtonActive{240, 196, 84, 255};
/// @brief Text on a highlighted button.
inline constexpr Color kButtonActiveText{40, 28, 6, 255};

/// @brief Main text colour.
inline constexpr Color kText{240, 244, 236, 255};
/// @brief Secondary text colour for labels.
inline constexpr Color kTextMuted{150, 196, 168, 255};
/// @brief Outline and mark of an empty pile.
inline constexpr Color kSlot{255, 255, 255, 60};
/// @brief Hint outline.
inline constexpr Color kHint{255, 214, 90, 255};
/// @brief Drop target outline while dragging.
inline constexpr Color kDropTarget{140, 230, 255, 255};
/// @brief "New best" and win text.
inline constexpr Color kGold{255, 214, 90, 255};
/// @brief Card drop shadow.
inline constexpr Color kShadow{0, 0, 0, 255};

}  // namespace solitaire

#endif  // SOLITAIRE_THEME_H_
