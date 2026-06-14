#pragma once
#include "Clr.h"
#include <SDL3/SDL.h>
#include <string>
#include <vector>

struct SelectableText {
    std::string text;
    int32_t     cursor    = 0;
    int32_t     sel_start = -1;
    bool        focused   = false;
    float       scroll_y  = 0.f;

    bool  ctx_open = false;
    float ctx_x = 0, ctx_y = 0;
    bool  dragging = false;

    struct Line { int32_t start, len; };
    std::vector<Line> lines;
    float box_x = 0, box_y = 0, box_w = 0, box_h = 0;

    void set(const std::string &s);
    void do_copy() const;
    bool handle_key(SDL_Keycode key, SDL_Keymod mod);
    void on_move(float mx, float my);
    void on_release();
    void on_scroll(float dy);
    bool mouse_over(float mx, float my) const;

    void render(SDL_Renderer *ren, float bx, float by, float bw, float bh,
                float mx, float my, bool ldown, bool rdown, Clr text_clr = C_TEXT);

private:
    void    build_lines();
    int32_t pos_at(float abs_x, int li) const;
    int32_t pos_at_mouse(float abs_x, float abs_y) const;
    float   total_h() const;
};
