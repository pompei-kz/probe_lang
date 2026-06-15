#pragma once
#include "Clr.h"
#include <SDL3/SDL.h>

namespace front {

  inline void sc(SDL_Renderer *r, Clr c)
  {
    SDL_SetRenderDrawColor(r, c.r, c.g, c.b, c.a);
  }

  inline void fill(SDL_Renderer *r, Clr c, float x, float y, float w, float h)
  {
    sc(r, c);
    SDL_FRect rc{x, y, w, h};
    SDL_RenderFillRect(r, &rc);
  }

  inline void rect(SDL_Renderer *r, Clr c, float x, float y, float w, float h)
  {
    sc(r, c);
    SDL_FRect rc{x, y, w, h};
    SDL_RenderRect(r, &rc);
  }

  inline bool hit(float mx, float my, float x, float y, float w, float h)
  {
    return mx >= x && mx < x + w && my >= y && my < y + h;
  }

} // namespace front
