#pragma once
#include "Conn.h"
#include "TextEditor.h"
#include <SDL3/SDL.h>
#include <string>

struct FormEditRepository {
    bool        open  = false;
    TextEditor  editors[2];   // [0]=schema name, [1]=repo name
    int         focus = 0;
    std::string err;
    Conn        conn;         // connection to create repository in

    void open_for(const Conn &c);

    // returns 0=open, 1=saved, -1=cancelled
    int render(SDL_Renderer *ren, float mx, float my, bool ldown, bool rdown, int clicks);
};
