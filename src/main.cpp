#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

#include <unicode/utf8.h>
#include <unicode/utypes.h>
#include <unicode/uchar.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <string>
#include <unordered_map>
#include <vector>

#include "resources.hpp"

namespace fs = std::filesystem;

// ── palette ──────────────────────────────────────────────────────────────────
struct Clr { Uint8 r, g, b, a = 255; };
constexpr Clr C_BG     {0x1e,0x1e,0x2e};
constexpr Clr C_PANEL  {0x18,0x18,0x28};
constexpr Clr C_BORDER {0x45,0x47,0x5a};
constexpr Clr C_HOVER  {0x2a,0x2b,0x3e};
constexpr Clr C_TEXT   {0xcd,0xd6,0xf4};
constexpr Clr C_DIM    {0x6c,0x70,0x86};
constexpr Clr C_ACCENT {0x89,0xb4,0xfa};
constexpr Clr C_INBG   {0x11,0x11,0x1b};
constexpr Clr C_DLGBG  {0x24,0x24,0x37};
constexpr Clr C_OVL    {0x00,0x00,0x00,0xaa};
constexpr Clr C_ERR    {0xf3,0x8b,0xa8};
constexpr Clr C_OK     {0xa6,0xe3,0xa1};

// ── font ─────────────────────────────────────────────────────────────────────
static constexpr float FS = 14.0f;

struct GlyphInfo {
    int   tx, ty, tw, th;
    int   bx, by;
    float adv;
    bool  visible;
};

struct FontAtlas {
    static constexpr int W = 2048, H = 2048, PAD = 1;

    SDL_Renderer*                          ren   = nullptr;
    SDL_Texture*                           tex   = nullptr;
    const SDL_PixelFormatDetails*          pfmt  = nullptr;
    std::unordered_map<UChar32, GlyphInfo> cache;

    stbtt_fontinfo info;
    float          scale = 1.f;
    float          asc   = 0.f;
    float          desc  = 0.f;

    int cx = PAD, cy = PAD, rh = 0;

    void init(SDL_Renderer* r, const unsigned char* ttf, float size) {
        ren = r;
        stbtt_InitFont(&info, ttf, stbtt_GetFontOffsetForIndex(ttf, 0));
        scale = stbtt_ScaleForPixelHeight(&info, size);
        int ai, di, lg;
        stbtt_GetFontVMetrics(&info, &ai, &di, &lg);
        asc  = ai * scale;
        desc = di * scale;

        pfmt = SDL_GetPixelFormatDetails(SDL_PIXELFORMAT_RGBA32);
        tex  = SDL_CreateTexture(r, SDL_PIXELFORMAT_RGBA32,
                                 SDL_TEXTUREACCESS_STATIC, W, H);
        SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);

        std::vector<Uint32> blank(W * H, 0);
        SDL_UpdateTexture(tex, nullptr, blank.data(), W * sizeof(Uint32));
    }

    const GlyphInfo& get(UChar32 cp) {
        auto it = cache.find(cp);
        if (it != cache.end()) return it->second;
        return bake(cp);
    }

private:
    const GlyphInfo& bake(UChar32 cp) {
        GlyphInfo g{};
        int gw, gh, gbx, gby;
        unsigned char* bmp = stbtt_GetCodepointBitmap(
            &info, 0, scale, static_cast<int>(cp), &gw, &gh, &gbx, &gby);

        if (bmp && gw > 0 && gh > 0) {
            if (cx + gw + PAD > W) { cx = PAD; cy += rh + PAD; rh = 0; }
            if (cy + gh + PAD <= H) {
                std::vector<Uint32> rgba(gw * gh);
                for (int i = 0; i < gw * gh; i++)
                    rgba[i] = SDL_MapRGBA(pfmt, nullptr, 255, 255, 255, bmp[i]);
                SDL_Rect r{cx, cy, gw, gh};
                SDL_UpdateTexture(tex, &r, rgba.data(), gw * sizeof(Uint32));

                g.tx = cx; g.ty = cy; g.tw = gw; g.th = gh;
                g.bx = gbx; g.by = gby;
                g.visible = true;
                cx += gw + PAD;
                rh  = std::max(rh, gh);
            }
        }

        int adv, lsb;
        stbtt_GetCodepointHMetrics(&info, static_cast<int>(cp), &adv, &lsb);
        g.adv = adv * scale;

        if (bmp) stbtt_FreeBitmap(bmp, nullptr);
        return cache.emplace(cp, g).first->second;
    }
};

static FontAtlas g_atlas;

static void font_init(SDL_Renderer* ren) {
    const auto* ttf = reinterpret_cast<const unsigned char*>(
        resources::fonts::Roboto_Regular_ttf.data());
    g_atlas.init(ren, ttf, FS);
}

static float text_draw(SDL_Renderer* ren, const char* s, float x, float y, Clr c) {
    SDL_SetTextureColorMod(g_atlas.tex, c.r, c.g, c.b);
    SDL_SetTextureAlphaMod(g_atlas.tex, c.a);
    float px = x;
    int32_t i = 0, len = static_cast<int32_t>(strlen(s));
    while (i < len) {
        UChar32 cp;
        U8_NEXT(reinterpret_cast<const uint8_t*>(s), i, len, cp);
        if (cp < 0) continue;
        const auto& g = g_atlas.get(cp);
        if (g.visible) {
            SDL_FRect src{(float)g.tx, (float)g.ty, (float)g.tw, (float)g.th};
            SDL_FRect dst{px + g.bx, y + g.by, (float)g.tw, (float)g.th};
            SDL_RenderTexture(ren, g_atlas.tex, &src, &dst);
        }
        px += g.adv;
    }
    return px - x;
}

static float text_w_n(const char* s, int32_t byte_len) {
    float px = 0;
    int32_t i = 0;
    while (i < byte_len) {
        UChar32 cp;
        U8_NEXT(reinterpret_cast<const uint8_t*>(s), i, byte_len, cp);
        if (cp < 0) continue;
        px += g_atlas.get(cp).adv;
    }
    return px;
}

static float text_w(const char* s) {
    return text_w_n(s, static_cast<int32_t>(strlen(s)));
}

static float center_baseline(float box_y, float box_h) {
    return box_y + (box_h + g_atlas.asc + g_atlas.desc) * 0.5f;
}

// ── SDL helpers ───────────────────────────────────────────────────────────────
static void sc(SDL_Renderer* r, Clr c) { SDL_SetRenderDrawColor(r, c.r, c.g, c.b, c.a); }
static void fill(SDL_Renderer* r, Clr c, float x, float y, float w, float h) {
    sc(r, c); SDL_FRect rc{x,y,w,h}; SDL_RenderFillRect(r, &rc);
}
static void rect(SDL_Renderer* r, Clr c, float x, float y, float w, float h) {
    sc(r, c); SDL_FRect rc{x,y,w,h}; SDL_RenderRect(r, &rc);
}
static bool hit(float mx, float my, float x, float y, float w, float h) {
    return mx>=x && mx<x+w && my>=y && my<y+h;
}

// ── icons ─────────────────────────────────────────────────────────────────────
static void draw_plus(SDL_Renderer* r, float cx, float cy, float sz, Clr c) {
    sc(r, c);
    SDL_FRect h{cx-sz*.55f, cy-sz*.12f, sz*1.1f, sz*.24f};
    SDL_FRect v{cx-sz*.12f, cy-sz*.55f, sz*.24f, sz*1.1f};
    SDL_RenderFillRect(r, &h);
    SDL_RenderFillRect(r, &v);
}
static void draw_pencil(SDL_Renderer* r, float cx, float cy, float sz, Clr c) {
    sc(r, c);
    for (int i = -1; i <= 1; i++) {
        float ox = (float)i;
        SDL_RenderLine(r, cx-sz*.3f+ox, cy+sz*.4f, cx+sz*.3f+ox, cy-sz*.4f);
    }
    SDL_RenderLine(r, cx-sz*.3f-1, cy+sz*.4f, cx-sz*.45f, cy+sz*.6f);
    SDL_RenderLine(r, cx-sz*.45f,  cy+sz*.6f, cx-sz*.05f, cy+sz*.45f);
    SDL_FRect cap{cx+sz*.15f, cy-sz*.55f, sz*.25f, sz*.15f};
    SDL_RenderFillRect(r, &cap);
}

// ── data ──────────────────────────────────────────────────────────────────────
static const char* PROG = "probe_lang";
static const char* EXT  = ".pg-connect";

struct Conn { std::string name, host, port, user, pass; };

static fs::path ws_dir() {
    const char* home = SDL_GetEnvironmentVariable(SDL_GetEnvironment(), "HOME");
    return fs::path(home ? home : ".") / ".config" / PROG / "workspace";
}

static std::vector<Conn> load_all() {
    std::vector<Conn> v;
    auto d = ws_dir();
    if (!fs::exists(d)) return v;
    std::error_code ec;
    for (auto& e : fs::directory_iterator(d, ec)) {
        if (e.path().extension() != EXT) continue;
        Conn c;
        c.name = e.path().stem().string();
        std::ifstream f(e.path());
        std::string ln;
        while (std::getline(f, ln)) {
            auto eq = ln.find('=');
            if (eq == std::string::npos) continue;
            auto k = ln.substr(0, eq), val = ln.substr(eq + 1);
            if      (k == "host") c.host = val;
            else if (k == "port") c.port = val;
            else if (k == "user") c.user = val;
            else if (k == "pass") c.pass = val;
        }
        v.push_back(std::move(c));
    }
    std::sort(v.begin(), v.end(), [](auto& a, auto& b){ return a.name < b.name; });
    return v;
}

static void save_conn(const Conn& c, const std::string& old_name = "") {
    auto d = ws_dir();
    fs::create_directories(d);
    if (!old_name.empty() && old_name != c.name)
        fs::remove(d / (old_name + EXT));
    std::ofstream f(d / (c.name + EXT));
    f << "host=" << c.host << "\nport=" << c.port
      << "\nuser=" << c.user << "\npass=" << c.pass << "\n";
}

// ── TextEditor ────────────────────────────────────────────────────────────────
enum class TxAction   { None, Insert, Delete, Other };
enum class DragState  { None, Pending, Active, MultiClick };

struct TxSnapshot { std::string buf; int32_t cursor, sel_start; };

struct TextEditor {
    std::string buf;
    int32_t  cursor    = 0;
    int32_t  sel_start = -1;
    float    view_px   = 0.f;
    bool     is_pwd    = false;

    std::vector<TxSnapshot> undo_stack;
    std::vector<TxSnapshot> redo_stack;
    TxAction  last_action = TxAction::None;

    // ── mouse drag state ──────────────────────────────────────────────────────
    DragState drag_state    = DragState::None;
    int32_t   press_bpos    = 0;    // byte pos at press
    float     press_abs_x   = 0.f;
    float     press_text_ox = 0.f;  // text_origin_x saved at press

    void set(const std::string& s) {
        buf = s;
        cursor = (int32_t)s.size();
        sel_start = -1; view_px = 0.f;
        undo_stack.clear(); redo_stack.clear();
        last_action = TxAction::None;
        drag_state  = DragState::None;
    }

    // ── undo ──────────────────────────────────────────────────────────────────
    void push_undo(TxAction action) {
        bool coalesce = (action == last_action) &&
                        (action == TxAction::Insert || action == TxAction::Delete) &&
                        sel_start < 0;
        if (!coalesce) {
            undo_stack.push_back({buf, cursor, sel_start});
            redo_stack.clear();
        }
        last_action = action;
    }

    void do_undo() {
        if (undo_stack.empty()) return;
        redo_stack.push_back({buf, cursor, sel_start});
        auto s = undo_stack.back(); undo_stack.pop_back();
        buf = s.buf; cursor = s.cursor; sel_start = s.sel_start;
        last_action = TxAction::None;
    }

    void do_redo() {
        if (redo_stack.empty()) return;
        undo_stack.push_back({buf, cursor, sel_start});
        auto s = redo_stack.back(); redo_stack.pop_back();
        buf = s.buf; cursor = s.cursor; sel_start = s.sel_start;
        last_action = TxAction::None;
    }

    // ── selection helpers ─────────────────────────────────────────────────────
    void delete_selection() {
        if (sel_start < 0) return;
        int32_t lo = std::min(sel_start, cursor);
        int32_t hi = std::max(sel_start, cursor);
        buf.erase((size_t)lo, (size_t)(hi - lo));
        cursor = lo; sel_start = -1;
    }

    void move_to(int32_t pos, bool shift) {
        if (shift) { if (sel_start < 0) sel_start = cursor; }
        else         sel_start = -1;
        cursor = std::clamp(pos, 0, (int32_t)buf.size());
    }

    void move_by(int dir, bool shift) {
        if (!shift && sel_start >= 0) {
            cursor = dir < 0 ? std::min(sel_start, cursor)
                             : std::max(sel_start, cursor);
            sel_start = -1;
            return;
        }
        int32_t np = cursor;
        if (dir < 0 && cursor > 0) {
            UChar32 cp;
            U8_PREV(reinterpret_cast<const uint8_t*>(buf.data()), 0, np, cp); (void)cp;
        } else if (dir > 0 && cursor < (int32_t)buf.size()) {
            UChar32 cp;
            U8_NEXT(reinterpret_cast<const uint8_t*>(buf.data()), np, (int32_t)buf.size(), cp); (void)cp;
        }
        move_to(np, shift);
    }

    int32_t word_left_pos() const {
        int32_t pos = cursor;
        const uint8_t* s = reinterpret_cast<const uint8_t*>(buf.data());
        while (pos > 0) { int32_t p=pos; UChar32 cp; U8_PREV(s,0,p,cp); if ( u_isalnum(cp)) break; pos=p; }
        while (pos > 0) { int32_t p=pos; UChar32 cp; U8_PREV(s,0,p,cp); if (!u_isalnum(cp)) break; pos=p; }
        return pos;
    }

    int32_t word_right_pos() const {
        int32_t pos=cursor, len=(int32_t)buf.size();
        const uint8_t* s = reinterpret_cast<const uint8_t*>(buf.data());
        while (pos<len) { int32_t p=pos; UChar32 cp; U8_NEXT(s,p,len,cp); if ( u_isalnum(cp)) break; pos=p; }
        while (pos<len) { int32_t p=pos; UChar32 cp; U8_NEXT(s,p,len,cp); if (!u_isalnum(cp)) break; pos=p; }
        return pos;
    }

    // ── clipboard ─────────────────────────────────────────────────────────────
    void do_copy() {
        if (sel_start < 0) return;
        int32_t lo = std::min(sel_start, cursor);
        int32_t hi = std::max(sel_start, cursor);
        SDL_SetClipboardText(buf.substr((size_t)lo, (size_t)(hi-lo)).c_str());
    }
    void do_cut()   { if (sel_start<0) return; do_copy(); push_undo(TxAction::Other); delete_selection(); }
    void do_paste() {
        char* clip = SDL_GetClipboardText();
        if (clip && *clip) {
            push_undo(TxAction::Other);
            delete_selection();
            int32_t tlen = (int32_t)strlen(clip);
            buf.insert((size_t)cursor, clip, (size_t)tlen);
            cursor += tlen; sel_start = -1;
        }
        SDL_free(clip);
    }

    // ── text input ────────────────────────────────────────────────────────────
    void handle_text(const char* text) {
        int32_t tlen = (int32_t)strlen(text);
        push_undo(tlen==1 && sel_start<0 ? TxAction::Insert : TxAction::Other);
        delete_selection();
        buf.insert((size_t)cursor, text, (size_t)tlen);
        cursor += tlen; sel_start = -1;
    }

    bool handle_key(SDL_Keycode key, SDL_Keymod mod) {
        bool ctrl  = (mod & SDL_KMOD_CTRL)  != 0;
        bool shift = (mod & SDL_KMOD_SHIFT) != 0;
        switch (key) {
        case SDLK_LEFT:  if (ctrl) move_to(word_left_pos(),  shift); else move_by(-1,shift); return true;
        case SDLK_RIGHT: if (ctrl) move_to(word_right_pos(), shift); else move_by(+1,shift); return true;
        case SDLK_HOME:  move_to(0,                   shift); return true;
        case SDLK_END:   move_to((int32_t)buf.size(), shift); return true;
        case SDLK_BACKSPACE:
            if (sel_start>=0)  { push_undo(TxAction::Other);  delete_selection(); }
            else if (ctrl)     { push_undo(TxAction::Other);
                                 int32_t t=word_left_pos(); buf.erase((size_t)t,(size_t)(cursor-t)); cursor=t; }
            else if (cursor>0) { push_undo(TxAction::Delete);
                                 int32_t p=cursor; UChar32 cp; U8_PREV(reinterpret_cast<const uint8_t*>(buf.data()),0,p,cp); (void)cp;
                                 buf.erase((size_t)p,(size_t)(cursor-p)); cursor=p; }
            return true;
        case SDLK_DELETE:
            if (ctrl)          { do_cut(); }
            else if (sel_start>=0) { push_undo(TxAction::Other); delete_selection(); }
            else if (cursor<(int32_t)buf.size()) {
                push_undo(TxAction::Delete);
                int32_t n=cursor; UChar32 cp; U8_NEXT(reinterpret_cast<const uint8_t*>(buf.data()),n,(int32_t)buf.size(),cp); (void)cp;
                buf.erase((size_t)cursor,(size_t)(n-cursor));
            }
            return true;
        case SDLK_A: if (ctrl) { sel_start=0; cursor=(int32_t)buf.size(); return true; } return false;
        case SDLK_C: if (ctrl) { do_copy();  return true; } return false;
        case SDLK_X: if (ctrl) { do_cut();   return true; } return false;
        case SDLK_V: if (ctrl) { do_paste(); return true; } return false;
        case SDLK_INSERT:
            if (ctrl)  { do_copy();  return true; }
            if (shift) { do_paste(); return true; }
            return false;
        case SDLK_Z:
            if (ctrl && shift) { do_redo(); return true; }
            if (ctrl)          { do_undo(); return true; }
            return false;
        case SDLK_Y: if (ctrl) { do_redo(); return true; } return false;
        default: return false;
        }
    }

    // ── password helpers ──────────────────────────────────────────────────────
    int32_t pwd_disp_off(int32_t byte_off) const {
        int32_t count=0, i=0, len=(int32_t)buf.size();
        const uint8_t* s = reinterpret_cast<const uint8_t*>(buf.data());
        while (i<byte_off && i<len) { UChar32 cp; U8_NEXT(s,i,len,cp); if (cp>=0) count++; }
        return count;
    }
    int32_t pwd_real_off(int32_t disp_off) const {
        int32_t count=0, i=0, len=(int32_t)buf.size();
        const uint8_t* s = reinterpret_cast<const uint8_t*>(buf.data());
        while (i<len && count<disp_off) { UChar32 cp; U8_NEXT(s,i,len,cp); if (cp>=0) count++; }
        return i;
    }
    std::string get_display() const {
        if (!is_pwd) return buf;
        int32_t count=0, i=0, len=(int32_t)buf.size();
        const uint8_t* s=reinterpret_cast<const uint8_t*>(buf.data());
        while (i<len) { UChar32 cp; U8_NEXT(s,i,len,cp); if (cp>=0) count++; }
        return std::string((size_t)count,'*');
    }
    int32_t disp_cursor()    const { return is_pwd ? pwd_disp_off(cursor)    : cursor; }
    int32_t disp_sel_start() const { return sel_start<0 ? -1
                                         : (is_pwd ? pwd_disp_off(sel_start) : sel_start); }

    // ── mouse interaction ─────────────────────────────────────────────────────
    // Byte position at absolute screen x (text_origin_x = left edge of text area).
    int32_t pos_at_x(float text_origin_x, float abs_x) const {
        float offset = abs_x - text_origin_x + view_px;
        std::string disp = get_display();
        const char* ds = disp.c_str();
        int32_t dlen=(int32_t)disp.size(), i=0, found=dlen;
        float cur_x = 0.f;
        while (i < dlen) {
            int32_t prev_i = i;
            UChar32 cp; U8_NEXT(reinterpret_cast<const uint8_t*>(ds), i, dlen, cp);
            if (cp < 0) continue;
            float adv = g_atlas.get(cp).adv;
            if (offset <= cur_x + adv*0.5f) { found = prev_i; break; }
            cur_x += adv;
        }
        return is_pwd ? pwd_real_off(found) : found;
    }

    // Range [lo,hi) of the word or whitespace-run that contains byte pos.
    std::pair<int32_t,int32_t> word_range_at(int32_t pos) const {
        int32_t len = (int32_t)buf.size();
        if (len == 0) return {0,0};
        const uint8_t* s = reinterpret_cast<const uint8_t*>(buf.data());
        UChar32 ref;
        if (pos < len) { int32_t p=pos; U8_NEXT(s,p,len,ref); }
        else           { int32_t p=pos; U8_PREV(s,0,p,ref);   }
        bool alnum = u_isalnum(ref) != 0;
        int32_t lo=pos, hi=pos;
        while (lo>0)  { int32_t p=lo; UChar32 cp; U8_PREV(s,0,p,cp); if ((u_isalnum(cp)!=0)!=alnum) break; lo=p; }
        while (hi<len){ int32_t p=hi; UChar32 cp; U8_NEXT(s,p,len,cp); if ((u_isalnum(cp)!=0)!=alnum) break; hi=p; }
        return {lo, hi};
    }

    void on_mouse_press(float text_origin_x, float abs_x, int clicks, bool shift) {
        press_abs_x   = abs_x;
        press_text_ox = text_origin_x;

        if (shift && clicks == 1) {
            // Shift+click: extend selection to clicked position
            move_to(pos_at_x(text_origin_x, abs_x), true);
            drag_state = DragState::Active; // allow shift+drag to continue extending
            return;
        }
        if (clicks >= 3) {
            sel_start  = 0; cursor = (int32_t)buf.size();
            drag_state = DragState::MultiClick;
            return;
        }
        if (clicks == 2) {
            auto [lo,hi] = word_range_at(pos_at_x(text_origin_x, abs_x));
            sel_start = lo; cursor = hi;
            drag_state = DragState::MultiClick;
            return;
        }
        press_bpos = pos_at_x(text_origin_x, abs_x);
        drag_state = DragState::Pending;
    }

    void on_mouse_move(float abs_x) {
        if (drag_state == DragState::None) return;
        if (drag_state == DragState::Pending) {
            float threshold = g_atlas.get('W').adv * 0.5f;
            if (std::abs(abs_x - press_abs_x) < threshold) return;
            drag_state = DragState::Active;
            cursor = press_bpos; sel_start = press_bpos;
        }
        // Active or MultiClick: extend cursor to current position
        cursor = pos_at_x(press_text_ox, abs_x);
    }

    void on_mouse_release() {
        if (drag_state == DragState::Pending) {
            // Threshold not crossed → plain click: place cursor, clear selection
            cursor = press_bpos; sel_start = -1;
        }
        drag_state = DragState::None;
    }

    // ── draw ─────────────────────────────────────────────────────────────────
    void draw(SDL_Renderer* ren, float bx, float by, float bw, float bh, bool focused) {
        fill(ren, C_INBG, bx, by, bw, bh);
        rect(ren, focused ? C_ACCENT : C_BORDER, bx, by, bw, bh);

        constexpr float PADX = 6.f;
        float inner_w = bw - 2.f*PADX;
        float text_y  = center_baseline(by, bh);

        std::string disp = get_display();
        const char* ds   = disp.c_str();
        int32_t     dlen = (int32_t)disp.size();
        int32_t     dc   = disp_cursor();
        int32_t     dss  = disp_sel_start();

        float total_w   = text_w_n(ds, dlen);
        float cursor_px = text_w_n(ds, dc);

        if (focused) {
            if (cursor_px - view_px < 0.f)
                view_px = cursor_px;
            else if (cursor_px - view_px > inner_w - 2.f)
                view_px = cursor_px - inner_w + 2.f;
        }
        view_px = std::clamp(view_px, 0.f, std::max(0.f, total_w - inner_w));

        float ox = bx + PADX - view_px;

        SDL_Rect clip{(int)(bx+1),(int)(by+1),(int)(bw-2),(int)(bh-2)};
        SDL_SetRenderClipRect(ren, &clip);

        // Selection highlight
        if (focused && dss >= 0) {
            int32_t lo = std::min(dss, dc), hi = std::max(dss, dc);
            float sx = ox + text_w_n(ds, lo);
            float sw = text_w_n(ds + lo, hi - lo);
            SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
            fill(ren, Clr{0x89,0xb4,0xfa,0x55}, sx, by+2.f, sw, bh-4.f);
            SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_NONE);
        }

        if (!disp.empty()) text_draw(ren, ds, ox, text_y, C_TEXT);

        // Blinking cursor
        if (focused && (SDL_GetTicks()/530)%2 == 0)
            fill(ren, C_TEXT, ox+cursor_px-0.5f, by+3.f, 1.5f, bh-6.f);

        SDL_SetRenderClipRect(ren, nullptr);
    }
};

// ── context menu ─────────────────────────────────────────────────────────────
struct ContextMenu {
    bool  open    = false;
    float x = 0, y = 0;
    int   ed_idx  = -1;

    static constexpr float W = 110.f, IH = 26.f;
    static constexpr int   N = 3;
    static constexpr const char* labels[N] = {"Copy","Cut","Paste"};

    // Returns 0=Copy, 1=Cut, 2=Paste, -1=no action. Closes itself on outside click.
    int render(SDL_Renderer* r, float mx, float my, bool ldown, bool rdown) {
        if (!open) return -1;
        float h = N*IH + 4.f;
        if ((ldown||rdown) && !hit(mx,my,x,y,W,h)) { open=false; return -1; }
        fill(r, C_DLGBG, x, y, W, h);
        rect(r, C_BORDER, x, y, W, h);
        int result = -1;
        for (int i = 0; i < N; i++) {
            float iy = y + 2.f + i*IH;
            bool hov = hit(mx,my,x,iy,W,IH);
            if (hov) fill(r, C_HOVER, x+1.f,iy,W-2.f,IH);
            text_draw(r, labels[i], x+12.f, center_baseline(iy,IH), C_TEXT);
            if (ldown && hov) { result=i; open=false; }
        }
        return result;
    }
};

// ── app state ─────────────────────────────────────────────────────────────────
struct Dlg {
    bool        open          = false;
    bool        editing       = false;
    std::string old_name;
    TextEditor  editors[5];
    int         focus         = 0;
    std::string err;
    int         active_drag_ed = -1;  // which editor currently owns mouse drag
    ContextMenu ctx_menu;

    void open_add() {
        for (int i=0;i<5;i++) { editors[i]=TextEditor{}; editors[i].is_pwd=(i==4); }
        focus=0; err=""; editing=false; old_name=""; active_drag_ed=-1; ctx_menu.open=false;
    }
    void open_edit(const Conn& c) {
        open_add();
        editors[0].set(c.name); editors[1].set(c.host); editors[2].set(c.port);
        editors[3].set(c.user); editors[4].set(c.pass);
        editing=true; old_name=c.name;
    }
    Conn to_conn() const {
        return {editors[0].buf,editors[1].buf,editors[2].buf,editors[3].buf,editors[4].buf};
    }
};

struct App {
    SDL_Window*   win  = nullptr;
    SDL_Renderer* ren  = nullptr;
    int ww=1280, wh=720;

    std::vector<Conn> conns;
    Dlg dlg;

    int  h_item=-1, h_edit=-1;
    bool h_add=false;
    float mx=0, my=0;
    bool lmb_held=false;  // left mouse button currently held
};

// ── dialog mouse event ───────────────────────────────────────────────────────
struct DlgMouse {
    float mx=0, my=0;
    bool  ldown=false, rdown=false;
    int   clicks=1;
};

// ── dialog ────────────────────────────────────────────────────────────────────
static constexpr float DW=440, DH=400, FH=28.f, FS_STEP=58.f;

// returns 0=open, 1=saved, -1=cancelled
static int dlg_render(SDL_Renderer* ren, Dlg& d, const DlgMouse& m) {
    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
    int ww, wh;
    SDL_GetCurrentRenderOutputSize(ren, &ww, &wh);
    fill(ren, C_OVL, 0,0,(float)ww,(float)wh);
    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_NONE);

    float dx=(ww-DW)*.5f, dy=(wh-DH)*.5f;
    fill(ren, C_DLGBG, dx,dy,DW,DH);
    rect(ren, C_BORDER, dx,dy,DW,DH);

    text_draw(ren, d.editing?"Edit Connection":"Add Connection", dx+16, dy+24, C_TEXT);
    sc(ren, C_BORDER);
    SDL_FRect sep{dx+1,dy+36,DW-2,1}; SDL_RenderFillRect(ren,&sep);

    struct FieldDef { const char* lbl; int idx; };
    constexpr FieldDef fields[5] = {{"Name",0},{"Host",1},{"Port",2},{"User",3},{"Password",4}};
    float fw=DW-32, ct=dy+48;
    float text_ox = dx+16.f+6.f; // x origin of text inside any field

    for (auto& f : fields) {
        float fy  = ct + f.idx*FS_STEP;
        float by  = fy + FS + 6;
        bool focused = (d.focus == f.idx);

        text_draw(ren, f.lbl, dx+16, fy+FS, C_DIM);
        d.editors[f.idx].draw(ren, dx+16, by, fw, FH, focused);

        // Left click in field (skip if context menu is consuming this click)
        if (m.ldown && !d.ctx_menu.open && hit(m.mx,m.my,dx+16,by,fw,FH)) {
            d.focus = f.idx;
            bool shift = (SDL_GetModState() & SDL_KMOD_SHIFT) != 0;
            d.editors[f.idx].on_mouse_press(text_ox, m.mx, m.clicks, shift);
            d.active_drag_ed = f.idx;
        }

        // Right click opens context menu
        if (m.rdown && hit(m.mx,m.my,dx+16,by,fw,FH)) {
            d.focus = f.idx;
            float cx = std::min(m.mx, dx+DW-ContextMenu::W-4.f);
            float cy = std::min(m.my, dy+DH-ContextMenu::N*ContextMenu::IH-10.f);
            d.ctx_menu = ContextMenu{true, cx, cy, f.idx};
        }
    }

    if (!d.err.empty())
        text_draw(ren, d.err.c_str(), dx+16, ct+5*FS_STEP, C_ERR);

    float btn_y=dy+DH-50;
    constexpr float BH=30,BW_S=90,BW_C=80;
    float sx=dx+DW-16-BW_S, cx2=sx-10-BW_C;

    bool h_save=hit(m.mx,m.my,sx,btn_y,BW_S,BH);
    bool h_can =hit(m.mx,m.my,cx2,btn_y,BW_C,BH);

    fill(ren, h_save?C_ACCENT:C_BORDER, sx, btn_y,BW_S,BH);
    fill(ren, h_can ?C_HOVER :C_BORDER, cx2,btn_y,BW_C,BH);

    auto btn_text=[&](const char* t,float bx,float bw,Clr c){
        text_draw(ren,t,bx+(bw-text_w(t))*.5f,center_baseline(btn_y,BH),c);
    };
    btn_text("Save",  sx, BW_S, h_save?C_PANEL:C_TEXT);
    btn_text("Cancel",cx2,BW_C, C_TEXT);

    // Context menu on top (last, so it's above everything)
    int menu_act = d.ctx_menu.render(ren, m.mx, m.my, m.ldown, m.rdown);
    if (menu_act >= 0 && d.ctx_menu.ed_idx >= 0) {
        auto& ed = d.editors[d.ctx_menu.ed_idx];
        if      (menu_act==0) ed.do_copy();
        else if (menu_act==1) ed.do_cut();
        else                  ed.do_paste();
    }

    if (m.ldown && !d.ctx_menu.open) {
        if (h_can) return -1;
        if (h_save) {
            Conn c=d.to_conn();
            if (c.name.empty()) { d.err="Name is required"; return 0; }
            if (c.host.empty()) { d.err="Host is required"; return 0; }
            return 1;
        }
    }
    return 0;
}

// ── left panel ───────────────────────────────────────────────────────────────
static constexpr float ITEM_H=30.f, HDR_H=38.f, PAD=10.f, ICON_SZ=8.f, EDIT_W=30.f;

static void panel_render(SDL_Renderer* ren, App& app, bool click) {
    float pw=app.ww*0.30f, ph=(float)app.wh;

    fill(ren, C_PANEL, 0,0,pw,ph);
    fill(ren, C_BG,    0,0,pw,HDR_H);
    text_draw(ren,"Connections",PAD,center_baseline(0,HDR_H),C_DIM);

    float abx=pw-HDR_H;
    bool h_add=hit(app.mx,app.my,abx,0,HDR_H,HDR_H);
    app.h_add=h_add;
    if (h_add) fill(ren,C_HOVER,abx,0,HDR_H,HDR_H);
    rect(ren,C_BORDER,abx,0,HDR_H,HDR_H);
    draw_plus(ren,abx+HDR_H*.5f,HDR_H*.5f,ICON_SZ,h_add?C_ACCENT:C_DIM);

    sc(ren,C_BORDER);
    SDL_FRect sep{0,HDR_H-1,pw,1}; SDL_RenderFillRect(ren,&sep);

    if (click && h_add) { app.dlg.open_add(); app.dlg.open=true; SDL_StartTextInput(app.win); }

    app.h_item=-1; app.h_edit=-1;
    for (int i=0;i<(int)app.conns.size();i++) {
        float iy=HDR_H+i*ITEM_H;
        bool h_row =hit(app.mx,app.my,0,       iy,pw-EDIT_W,ITEM_H);
        bool h_edit=hit(app.mx,app.my,pw-EDIT_W,iy,EDIT_W,  ITEM_H);
        if (h_row)  app.h_item=i;
        if (h_edit) app.h_edit=i;
        if (h_row||h_edit) fill(ren,C_HOVER,0,iy,pw,ITEM_H);
        text_draw(ren,app.conns[i].name.c_str(),PAD,center_baseline(iy,ITEM_H),C_TEXT);
        draw_pencil(ren,pw-EDIT_W*.5f,iy+ITEM_H*.5f,ICON_SZ,h_edit?C_ACCENT:C_DIM);
        sc(ren,C_BORDER);
        SDL_FRect line{0,iy+ITEM_H-1,pw,1}; SDL_RenderFillRect(ren,&line);
        if (click && h_edit) {
            app.dlg.open_edit(app.conns[i]);
            app.dlg.open=true;
            SDL_StartTextInput(app.win);
        }
    }
    sc(ren,C_BORDER);
    SDL_FRect div{pw-1,0,1,ph}; SDL_RenderFillRect(ren,&div);
}

// ── main ──────────────────────────────────────────────────────────────────────
int main(int /*argc*/, char* /*argv*/[]) {
    if (!SDL_Init(SDL_INIT_VIDEO)) { SDL_Log("SDL_Init: %s",SDL_GetError()); return 1; }

    App app;
    app.win=SDL_CreateWindow("probe_lang",app.ww,app.wh,SDL_WINDOW_RESIZABLE);
    if (!app.win) { SDL_Log("CreateWindow: %s",SDL_GetError()); return 1; }
    app.ren=SDL_CreateRenderer(app.win,nullptr);
    if (!app.ren) { SDL_Log("CreateRenderer: %s",SDL_GetError()); return 1; }

    SDL_SetRenderDrawBlendMode(app.ren, SDL_BLENDMODE_NONE);
    font_init(app.ren);
    app.conns=load_all();

    if (SDL_GetEnvironmentVariable(SDL_GetEnvironment(),"PROBE_TEST_DIALOG")) {
        app.dlg.open_add(); app.dlg.open=true; SDL_StartTextInput(app.win);
    } else if (SDL_GetEnvironmentVariable(SDL_GetEnvironment(),"PROBE_TEST_EDIT") && !app.conns.empty()) {
        app.dlg.open_edit(app.conns[0]); app.dlg.open=true; SDL_StartTextInput(app.win);
    }

    bool running=true;
    while (running) {
        // Per-frame event accumulators
        bool  lclick=false, rclick=false;
        float lclick_x=0, lclick_y=0, rclick_x=0, rclick_y=0;
        int   lclicks=1;

        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            switch (ev.type) {
            case SDL_EVENT_QUIT: running=false; break;

            case SDL_EVENT_WINDOW_RESIZED:
                SDL_GetWindowSize(app.win,&app.ww,&app.wh); break;

            case SDL_EVENT_MOUSE_MOTION:
                app.mx=ev.motion.x; app.my=ev.motion.y;
                if (app.dlg.open && app.dlg.active_drag_ed>=0 && app.lmb_held)
                    app.dlg.editors[app.dlg.active_drag_ed].on_mouse_move(ev.motion.x);
                break;

            case SDL_EVENT_MOUSE_BUTTON_DOWN:
                if (ev.button.button==SDL_BUTTON_LEFT) {
                    app.lmb_held=true;
                    lclick=true; lclick_x=ev.button.x; lclick_y=ev.button.y;
                    lclicks=ev.button.clicks;
                }
                if (ev.button.button==SDL_BUTTON_RIGHT) {
                    rclick=true; rclick_x=ev.button.x; rclick_y=ev.button.y;
                }
                break;

            case SDL_EVENT_MOUSE_BUTTON_UP:
                if (ev.button.button==SDL_BUTTON_LEFT) {
                    app.lmb_held=false;
                    if (app.dlg.open && app.dlg.active_drag_ed>=0) {
                        app.dlg.editors[app.dlg.active_drag_ed].on_mouse_release();
                        app.dlg.active_drag_ed=-1;
                    }
                }
                break;

            case SDL_EVENT_TEXT_INPUT:
                if (app.dlg.open)
                    app.dlg.editors[app.dlg.focus].handle_text(ev.text.text);
                break;

            case SDL_EVENT_KEY_DOWN:
                if (app.dlg.open) {
                    app.dlg.ctx_menu.open=false;
                    SDL_Keymod mod=ev.key.mod;
                    bool consumed=app.dlg.editors[app.dlg.focus].handle_key(ev.key.key,mod);
                    if (!consumed) {
                        bool shift=(mod&SDL_KMOD_SHIFT)!=0;
                        switch (ev.key.key) {
                        case SDLK_TAB:
                            app.dlg.focus=(app.dlg.focus+(shift?4:1))%5; break;
                        case SDLK_ESCAPE:
                            app.dlg.open=false; SDL_StopTextInput(app.win); break;
                        case SDLK_RETURN: case SDLK_KP_ENTER:
                            if (app.dlg.focus<4) { app.dlg.focus++; }
                            else {
                                Conn c=app.dlg.to_conn();
                                if (!c.name.empty()&&!c.host.empty()) {
                                    save_conn(c,app.dlg.old_name);
                                    app.conns=load_all();
                                    app.dlg.open=false; SDL_StopTextInput(app.win);
                                } else {
                                    app.dlg.err=c.name.empty()?"Name is required":"Host is required";
                                }
                            }
                            break;
                        default: break;
                        }
                    }
                }
                break;

            default: break;
            }
        }

        // ── render ────────────────────────────────────────────────────────────
        sc(app.ren, C_BG); SDL_RenderClear(app.ren);

        panel_render(app.ren, app, lclick);

        float pw=app.ww*0.30f;
        fill(app.ren, C_BG, pw,0,app.ww-pw,(float)app.wh);

        if (app.dlg.open) {
            DlgMouse dm;
            dm.mx     = lclick ? lclick_x : (rclick ? rclick_x : app.mx);
            dm.my     = lclick ? lclick_y : (rclick ? rclick_y : app.my);
            dm.ldown  = lclick;
            dm.rdown  = rclick;
            dm.clicks = lclicks;

            int result=dlg_render(app.ren,app.dlg,dm);
            if (result==1) {
                save_conn(app.dlg.to_conn(),app.dlg.old_name);
                app.conns=load_all(); app.dlg.open=false; SDL_StopTextInput(app.win);
            } else if (result==-1) {
                app.dlg.open=false; SDL_StopTextInput(app.win);
            }
        }

        SDL_RenderPresent(app.ren);
        SDL_Delay(16);
    }

    SDL_DestroyRenderer(app.ren);
    SDL_DestroyWindow(app.win);
    SDL_Quit();
    return 0;
}
