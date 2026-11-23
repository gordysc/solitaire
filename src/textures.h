#ifndef SOLITAIRE_TEXTURES_H_
#define SOLITAIRE_TEXTURES_H_

#include <raylib.h>

#include <array>

#include "card.h"
#include "storage.h"

/// @brief Everything that belongs to the Solitaire game.
namespace solitaire {

/// @brief Owns the card images: 52 fronts and the card back designs.
///
/// RAII: the constructor loads every image from `assets/cards/` next to the
/// executable and uploads it to the GPU; the destructor frees them. It must
/// be created after `InitWindow()` (textures need a graphics context) and
/// destroyed before `CloseWindow()`.
///
/// If an image is missing, its texture stays empty (`id == 0`) and the
/// renderer draws a plain stand-in card instead, so the game still runs.
class CardTextures {
 public:
  /// @brief Loads every card image.
  CardTextures();

  /// @brief Frees every card image.
  ~CardTextures();

  /// @brief Non-copyable: a copy would free the same GPU textures twice.
  CardTextures(const CardTextures&) = delete;
  /// @brief Non-copy-assignable for the same reason.
  CardTextures& operator=(const CardTextures&) = delete;

  /// @brief Returns the face image of `card`.
  /// @param card Card to look up (its `face_up` flag is ignored).
  /// @return The texture; `id == 0` if the image failed to load.
  const Texture2D& Front(const Card& card) const;

  /// @brief Returns a card back design.
  /// @param design Design number (`0..kCardBackCount-1`, wraps around).
  /// @return The texture; `id == 0` if the image failed to load.
  const Texture2D& Back(int design) const;

 private:
  /// @brief Faces indexed by `CardId()`.
  std::array<Texture2D, kDeckSize> fronts_{};
  /// @brief Back designs.
  std::array<Texture2D, kCardBackCount> backs_{};
};

}  // namespace solitaire

#endif  // SOLITAIRE_TEXTURES_H_
