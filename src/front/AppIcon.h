#pragma once
#include <SDL3/SDL.h>

namespace front {

  // Build the application/window icon: a little hedgehog (ёжик), drawn
  // procedurally — no image resource. The caller owns the returned surface and
  // must SDL_DestroySurface it (after SDL_SetWindowIcon has copied it).
  // Возвращает nullptr при ошибке выделения поверхности.
  SDL_Surface *make_hedgehog_icon();

} // namespace front
