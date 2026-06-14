#pragma once
#include <SDL3/SDL.h>
#include <string>

struct ConfirmDlg {
    bool        open  = false;
    std::string title;
    std::string msg;

    // returns 1=Да, -1=Нет/закрыт, 0=ещё открыт
    int render(SDL_Renderer *ren, float mx, float my, bool ldown);
};
