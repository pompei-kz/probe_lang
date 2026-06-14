#pragma once
#include <SDL3/SDL.h>

struct PanelMenu {
    bool  open      = false;
    float x = 0, y = 0;
    int   conn_idx  = -1;
    bool  connected = false;

    static constexpr float W  = 160.f;
    static constexpr float IH = 26.f;
    static constexpr int   N  = 5;

    // returns 0=Присоединиться/Отсоединиться, 1=AddRepo, 2=Add, 3=Edit, 4=Delete, -1=none
    int render(SDL_Renderer *r, float mx, float my, bool ldown, bool rdown);
};
