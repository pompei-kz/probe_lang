#include "AppIcon.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include "resources.hpp"

#include <cstdint>
#include <cstring>

namespace front {

  // Decode the embedded PNG (resources/application_icon.png → a binary resource,
  // resources::application_icon_png) into an RGBA SDL surface for the window icon.
  SDL_Surface *make_hedgehog_icon()
  {
    const auto data = resources::application_icon_png;

    int      w = 0, h = 0, channels = 0;
    stbi_uc *pixels = stbi_load_from_memory(data.data(), static_cast<int>(data.size()), &w, &h, &channels, 4);
    if (!pixels) return nullptr;

    SDL_Surface *surf = SDL_CreateSurface(w, h, SDL_PIXELFORMAT_RGBA32);
    if (!surf) {
      stbi_image_free(pixels);
      return nullptr;
    }

    auto *dst = static_cast<uint8_t *>(surf->pixels);
    for (int y = 0; y < h; y++) std::memcpy(dst + static_cast<size_t>(y) * surf->pitch, pixels + static_cast<size_t>(y) * w * 4, static_cast<size_t>(w) * 4);

    stbi_image_free(pixels);
    return surf;
  }

} // namespace front
