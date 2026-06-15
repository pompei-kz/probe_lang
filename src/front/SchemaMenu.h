#pragma once
#include "MenuNav.h"
#include <SDL3/SDL.h>

namespace front {

  struct RepoMenu
  {
    bool  open = false;
    float x = 0, y = 0;
    int   conn_idx = -1;
    int   repo_idx = -1;
    int   hi       = -1;
    int   pending  = -1;

    static constexpr float W  = 210.f;
    static constexpr float IH = 26.f;
    static constexpr int   N  = 3;

    void key(SDL_Keycode k) { menu_nav_key(N, hi, pending, open, k); }

    // returns 0=Изменить репозиторий, 1=Добавить папку, 2=Добавить юнит, -1=none
    int render(SDL_Renderer *r, float mx, float my, bool ldown, bool rdown);
  };

} // namespace front
