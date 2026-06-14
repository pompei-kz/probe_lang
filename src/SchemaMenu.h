#pragma once
#include <SDL3/SDL.h>

struct RepoMenu {
    bool  open     = false;
    float x = 0,  y = 0;
    int   conn_idx = -1;
    int   repo_idx = -1;

    static constexpr float W  = 210.f;
    static constexpr float IH = 26.f;
    static constexpr int   N  = 2;

    // returns 0=Изменить репозиторий, 1=Добавить папку, -1=none
    int render(SDL_Renderer *r, float mx, float my, bool ldown, bool rdown);
};
