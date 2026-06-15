#pragma once
#include <SDL3/SDL.h>

namespace front {

  // Shared keyboard navigation for the popup menus.
  //  - Up/Down move the highlighted item `hi` (clamped to [0, n-1]).
  //  - Enter requests activation of `hi` via `pending` (consumed by render()).
  //  - Escape closes the menu.
  inline void menu_nav_key(int n, int &hi, int &pending, bool &open, SDL_Keycode k)
  {
    if (k == SDLK_DOWN)
      hi = (hi < 0) ? 0 : (hi + 1 >= n ? n - 1 : hi + 1);
    else if (k == SDLK_UP)
      hi = (hi <= 0) ? 0 : hi - 1;
    else if (k == SDLK_RETURN || k == SDLK_KP_ENTER) {
      if (hi >= 0 && hi < n) pending = hi;
    } else if (k == SDLK_ESCAPE)
      open = false;
  }

} // namespace front
