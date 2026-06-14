#pragma once
#include <SDL3/SDL.h>

struct PanelMenu {
    bool  open     = false;
    float x = 0, y = 0;
    int   conn_idx = -1;

    static constexpr float       W        = 110.f;
    static constexpr float       IH       = 26.f;
    static constexpr int         N        = 3;
    static constexpr const char *labels[N] = {"Add", "Edit", "Delete"};

    // returns 0=Add, 1=Edit, 2=Delete, -1=none
    int render(SDL_Renderer *r, float mx, float my, bool ldown, bool rdown);
};
