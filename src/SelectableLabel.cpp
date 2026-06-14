#include "SelectableLabel.h"
#include "FontAtlas.h"
#include "render_helpers.h"
#include <SDL3/SDL.h>
#include <algorithm>
#include <unicode/uchar.h>
#include <unicode/utf8.h>

void SelectableLabel::set(const std::string &s)
{
    text      = s;
    sel_start = -1;
    cursor    = 0;
    dragging  = false;
    ctx_open  = false;
}

void SelectableLabel::do_copy() const
{
    if (sel_start < 0 || sel_start == cursor) return;
    int32_t lo = std::min(sel_start, cursor);
    int32_t hi = std::max(sel_start, cursor);
    SDL_SetClipboardText(text.substr((size_t)lo, (size_t)(hi - lo)).c_str());
}

bool SelectableLabel::handle_key(SDL_Keycode key, SDL_Keymod mod) const
{
    if (sel_start < 0 || sel_start == cursor) return false;
    bool ctrl = (mod & SDL_KMOD_CTRL) != 0;
    if ((key == SDLK_C && ctrl) || (key == SDLK_INSERT && ctrl)) {
        do_copy();
        return true;
    }
    return false;
}

int32_t SelectableLabel::pos_at_x(float origin_x, float abs_x) const
{
    float       offset = abs_x - origin_x;
    const char *s      = text.c_str();
    int32_t     len    = (int32_t)text.size(), i = 0, found = len;
    float       cur_x  = 0.f;
    while (i < len) {
        int32_t prev_i = i;
        UChar32 cp;
        U8_NEXT(reinterpret_cast<const uint8_t *>(s), i, len, cp);
        if (cp < 0) continue;
        float adv = g_atlas.get(cp).adv;
        if (offset <= cur_x + adv * 0.5f) { found = prev_i; break; }
        cur_x += adv;
    }
    return found;
}

void SelectableLabel::render(SDL_Renderer *ren, float x, float y, Clr c,
                             float mx, float my, bool ldown, bool lmb_held, bool rdown)
{
    if (text.empty()) { ctx_open = false; return; }

    const char *s   = text.c_str();
    int32_t     len = (int32_t)text.size();
    float       tw  = text_w_n(s, len);
    bool        over = hit(mx, my, x, y - FS, tw, FS * 1.3f);

    if (!lmb_held) dragging = false;

    // context menu click (process before text interaction)
    bool menu_consumed = false;
    if (ctx_open && ldown) {
        constexpr float CW = 130.f, CH = 28.f;
        if (hit(mx, my, ctx_x, ctx_y, CW, CH)) {
            if (sel_start < 0 || sel_start == cursor) { sel_start = 0; cursor = len; }
            do_copy();
            menu_consumed = true;
        }
        ctx_open = false;
    }

    if (!menu_consumed) {
        if (ldown && over) {
            int32_t pos = pos_at_x(x, mx);
            sel_start   = pos;
            cursor      = pos;
            dragging    = true;
        } else if (lmb_held && dragging) {
            cursor = pos_at_x(x, mx);
        } else if (ldown) {
            sel_start = -1;
        }
    }

    if (rdown && over) {
        ctx_open = true;
        ctx_x    = mx;
        ctx_y    = my;
    }

    // selection highlight
    if (sel_start >= 0 && sel_start != cursor) {
        int32_t lo = std::min(sel_start, cursor);
        int32_t hi = std::max(sel_start, cursor);
        float   sx = x + text_w_n(s, lo);
        float   sw = text_w_n(s + lo, hi - lo);
        SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
        fill(ren, Clr{0x89, 0xb4, 0xfa, 0x55}, sx, y - FS, sw, FS * 1.3f);
        SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_NONE);
    }

    text_draw(ren, s, x, y, c);

    // context menu rendered on top
    if (ctx_open) {
        constexpr float CW = 130.f, CH = 28.f;
        bool hov = hit(mx, my, ctx_x, ctx_y, CW, CH);
        fill(ren, C_DLGBG, ctx_x, ctx_y, CW, CH);
        rect(ren, C_BORDER, ctx_x, ctx_y, CW, CH);
        if (hov) fill(ren, C_HOVER, ctx_x + 1.f, ctx_y, CW - 2.f, CH);
        text_draw(ren, "Скопировать", ctx_x + 10.f, center_baseline(ctx_y, CH), C_TEXT);
    }
}
