#pragma once
#include "Clr.h"
#include <SDL3/SDL.h>
#include <string>

struct SelectableLabel {
    std::string text;
    int32_t     sel_start = -1;
    int32_t     cursor    = 0;
    bool        dragging  = false;
    bool        ctx_open  = false;
    float       ctx_x = 0, ctx_y = 0;

    void set(const std::string &s);
    void do_copy() const;

    // returns true if it consumed the key shortcut (copy)
    bool handle_key(SDL_Keycode key, SDL_Keymod mod) const;

    void render(SDL_Renderer *ren, float x, float y, Clr c,
                float mx, float my, bool ldown, bool lmb_held, bool rdown);

private:
    int32_t pos_at_x(float origin_x, float abs_x) const;
};
