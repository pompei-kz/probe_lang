#pragma once
#include <SDL3/SDL.h>
#include <string>

struct MsgDlg {
    bool        open = false;
    std::string title;
    std::string msg;

    // returns true when OK clicked (caller should set open=false)
    bool render(SDL_Renderer *ren, float mx, float my, bool ldown);
};
