#include "textures.h"

#include <string>

/// @brief Everything that belongs to the Solitaire game.
namespace solitaire {

/// @brief Loading helpers internal to `textures.cpp`.
namespace {

/// @brief File names of the card back designs, in `assets/cards/`.
constexpr std::array<const char*, kCardBackCount> kBackFiles{
    "back_blue.png", "back_red.png", "back_blue_alt.png", "back_red_alt.png"};

/// @brief Loads one image as a smooth, mipmapped texture.
///
/// The images are larger than the cards on screen, so the GPU shrinks
/// them. Mipmaps are pre-shrunk copies at half, quarter, ... size; with
/// trilinear filtering the GPU blends between them, which keeps the fine
/// lines of the court cards from shimmering.
/// @param file File name inside `assets/cards/`.
/// @return The texture, or an empty one (`id == 0`) if the file is missing.
Texture2D LoadCardTexture(const char* file) {
  // `GetApplicationDirectory()` is the folder that holds the executable
  // (with a trailing slash), so the game finds its images wherever it is
  // started from. CMake copies `assets/` there after every build.
  const std::string path =
      std::string(GetApplicationDirectory()) + "assets/cards/" + file;
  if (!FileExists(path.c_str())) return Texture2D{};
  Texture2D texture = LoadTexture(path.c_str());
  if (texture.id == 0) return texture;
  // `GenTextureMipmaps` takes a pointer because it writes the new mipmap
  // count back into the struct.
  GenTextureMipmaps(&texture);
  SetTextureFilter(texture, TEXTURE_FILTER_TRILINEAR);
  return texture;
}

}  // namespace

CardTextures::CardTextures() {
  for (int s = 0; s < kSuitCount; ++s) {
    for (int rank = kAce; rank <= kKing; ++rank) {
      const Card card{static_cast<Suit>(s), rank, true};
      // File names look like `queen_of_hearts.png`.
      const std::string file =
          std::string(RankName(rank)) + "_of_" + SuitName(card.suit) + ".png";
      fronts_[static_cast<std::size_t>(CardId(card))] =
          LoadCardTexture(file.c_str());
    }
  }
  for (std::size_t i = 0; i < backs_.size(); ++i) {
    backs_[i] = LoadCardTexture(kBackFiles[i]);
  }
}

CardTextures::~CardTextures() {
  // `UnloadTexture` frees GPU memory. Skipping empty slots avoids asking
  // raylib to free textures that were never created.
  for (const Texture2D& texture : fronts_) {
    if (texture.id != 0) UnloadTexture(texture);
  }
  for (const Texture2D& texture : backs_) {
    if (texture.id != 0) UnloadTexture(texture);
  }
}

const Texture2D& CardTextures::Front(const Card& card) const {
  return fronts_[static_cast<std::size_t>(CardId(card))];
}

const Texture2D& CardTextures::Back(int design) const {
  // `((x % n) + n) % n` wraps negative numbers too, unlike a plain `%`.
  const int wrapped =
      ((design % kCardBackCount) + kCardBackCount) % kCardBackCount;
  return backs_[static_cast<std::size_t>(wrapped)];
}

}  // namespace solitaire
