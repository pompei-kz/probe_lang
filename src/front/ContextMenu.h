#pragma once
#include <SDL3/SDL.h>

namespace front {

  struct ContextMenu
  {
    bool  open   = false;
    float x      = 0;
    float y      = 0;
    int   ed_idx = -1;

    static constexpr float       W         = 110.f;
    static constexpr float       IH        = 26.f;
    static constexpr int         N         = 3;
    static constexpr const char *labels[N] = {"Copy", "Cut", "Paste"};

    // Returns 0=Copy, 1=Cut, 2=Paste, -1=no action. Closes itself on outside click.
    int render(SDL_Renderer *r, float mx, float my, bool ldown, bool rdown);
  };

} // namespace front
