#pragma once
#include <SDL3/SDL.h>

struct SchemaMenu {
    bool  open       = false;
    float x = 0, y  = 0;
    int   conn_idx   = -1;
    int   schema_idx = -1;

    static constexpr float W  = 160.f;
    static constexpr float IH = 26.f;
    static constexpr int   N  = 1;

    // returns 0=Изменить схему, -1=none
    int render(SDL_Renderer *r, float mx, float my, bool ldown, bool rdown);
};
