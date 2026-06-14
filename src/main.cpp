#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

#include <unicode/utf8.h>
#include <unicode/utypes.h>

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

// One entry per unique codepoint, added on first use.
struct GlyphInfo {
    int   tx, ty, tw, th; // position in atlas texture (pixels)
    int   bx, by;         // bearing: offset from pen-origin to glyph top-left
    float adv;            // horizontal advance
    bool  visible;        // false for whitespace / missing glyphs
};

struct FontAtlas {
    // Atlas size. 2048×2048 holds thousands of glyphs at 14 px.
    static constexpr int W = 2048, H = 2048, PAD = 1;

    SDL_Renderer*                          ren   = nullptr;
    SDL_Texture*                           tex   = nullptr;
    const SDL_PixelFormatDetails*          pfmt  = nullptr;
    std::unordered_map<UChar32, GlyphInfo> cache;

    stbtt_fontinfo info;
    float          scale = 1.f;
    float          asc   = 0.f; // ascent  (positive, pixels above baseline)
    float          desc  = 0.f; // descent (negative, pixels below baseline)

    // Row-advance packer state
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

        // Clear atlas to fully transparent
        std::vector<Uint32> blank(W * H, 0);
        SDL_UpdateTexture(tex, nullptr, blank.data(), W * sizeof(Uint32));
    }

    // Returns cached glyph, rasterizing it on first access.
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
            // Advance to next row if this glyph doesn't fit horizontally
            if (cx + gw + PAD > W) { cx = PAD; cy += rh + PAD; rh = 0; }

            if (cy + gh + PAD <= H) {
                // Convert 8-bit alpha bitmap → RGBA and upload the subrect
                std::vector<Uint32> rgba(gw * gh);
                for (int i = 0; i < gw * gh; i++)
                    rgba[i] = SDL_MapRGBA(pfmt, nullptr, 255, 255, 255, bmp[i]);
                SDL_Rect r{cx, cy, gw, gh};
                SDL_UpdateTexture(tex, &r, rgba.data(), gw * sizeof(Uint32));

                g.tx = cx;  g.ty = cy;
                g.tw = gw;  g.th = gh;
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

// x = left edge, y = baseline.
// Iterates UTF-8 via ICU U8_NEXT; rasterizes unseen glyphs on demand.
static float text_draw(SDL_Renderer* ren, const char* s, float x, float y, Clr c) {
    SDL_SetTextureColorMod(g_atlas.tex, c.r, c.g, c.b);
    SDL_SetTextureAlphaMod(g_atlas.tex, c.a);
    float px = x;
    int32_t i = 0, len = static_cast<int32_t>(strlen(s));
    while (i < len) {
        UChar32 cp;
        U8_NEXT(reinterpret_cast<const uint8_t*>(s), i, len, cp);
        if (cp < 0) continue; // invalid UTF-8 byte
        const auto& g = g_atlas.get(cp);
        if (g.visible) {
            SDL_FRect src{static_cast<float>(g.tx), static_cast<float>(g.ty),
                          static_cast<float>(g.tw), static_cast<float>(g.th)};
            SDL_FRect dst{px + g.bx, y + g.by,
                          static_cast<float>(g.tw), static_cast<float>(g.th)};
            SDL_RenderTexture(ren, g_atlas.tex, &src, &dst);
        }
        px += g.adv;
    }
    return px - x;
}

static float text_w(const char* s) {
    float px = 0;
    int32_t i = 0, len = static_cast<int32_t>(strlen(s));
    while (i < len) {
        UChar32 cp;
        U8_NEXT(reinterpret_cast<const uint8_t*>(s), i, len, cp);
        if (cp < 0) continue;
        px += g_atlas.get(cp).adv;
    }
    return px;
}

// Baseline y for text vertically centred inside a box (top=box_y, height=box_h).
static float center_baseline(float box_y, float box_h) {
    return box_y + (box_h + g_atlas.asc + g_atlas.desc) * 0.5f;
}

// ── SDL helpers ───────────────────────────────────────────────────────────────
static void sc(SDL_Renderer* r, Clr c) {
    SDL_SetRenderDrawColor(r, c.r, c.g, c.b, c.a);
}
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
    SDL_FRect h {cx - sz*.55f, cy - sz*.12f, sz*1.1f, sz*.24f};
    SDL_FRect v {cx - sz*.12f, cy - sz*.55f, sz*.24f, sz*1.1f};
    SDL_RenderFillRect(r, &h);
    SDL_RenderFillRect(r, &v);
}

static void draw_pencil(SDL_Renderer* r, float cx, float cy, float sz, Clr c) {
    sc(r, c);
    // body: diagonal stroke (3 lines offset by 1 px)
    for (int i = -1; i <= 1; i++) {
        float ox = (float)i;
        SDL_RenderLine(r, cx-sz*.3f+ox, cy+sz*.4f, cx+sz*.3f+ox, cy-sz*.4f);
    }
    // tip triangle
    SDL_RenderLine(r, cx-sz*.3f-1, cy+sz*.4f, cx-sz*.45f, cy+sz*.6f);
    SDL_RenderLine(r, cx-sz*.45f,  cy+sz*.6f, cx-sz*.05f, cy+sz*.45f);
    // eraser cap
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

// ── app state ─────────────────────────────────────────────────────────────────
struct Dlg {
    bool        open     = false;
    bool        editing  = false;
    std::string old_name;       // name before edit (for rename)
    Conn        c;
    int         focus    = 0;   // 0-4: name/host/port/user/pass
    std::string err;
};

struct App {
    SDL_Window*   win  = nullptr;
    SDL_Renderer* ren  = nullptr;
    int ww = 1280, wh = 720;

    std::vector<Conn> conns;
    Dlg dlg;

    int  h_item = -1; // hovered tree item index
    int  h_edit = -1; // hovered edit-pencil item index
    bool h_add  = false;

    float mx = 0, my = 0;
};

// ── dialog ───────────────────────────────────────────────────────────────────
static constexpr float DW = 440, DH = 400;
static constexpr float FH = 28.0f;  // input field height
static constexpr float FS_STEP = 58.0f; // vertical step per field

static std::string* dlg_field(Dlg& d, int i) {
    switch (i) {
        case 0: return &d.c.name;
        case 1: return &d.c.host;
        case 2: return &d.c.port;
        case 3: return &d.c.user;
        case 4: return &d.c.pass;
        default: return nullptr;
    }
}

// returns 0=open, 1=saved, -1=cancelled
static int dlg_render(SDL_Renderer* ren, Dlg& d, float mx, float my, bool click) {
    // The window dimensions are retrieved via SDL_GetWindowSize before this call

    // Overlay
    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
    int ww, wh;
    SDL_GetCurrentRenderOutputSize(ren, &ww, &wh);
    fill(ren, C_OVL, 0, 0, (float)ww, (float)wh);
    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_NONE);

    float dx = (ww - DW) * .5f, dy = (wh - DH) * .5f;
    fill(ren, C_DLGBG, dx, dy, DW, DH);
    rect(ren, C_BORDER, dx, dy, DW, DH);

    // Title
    const char* title = d.editing ? "Edit Connection" : "Add Connection";
    text_draw(ren, title, dx + 16, dy + 24, C_TEXT);
    sc(ren, C_BORDER);
    SDL_FRect sep{dx+1, dy+36, DW-2, 1};
    SDL_RenderFillRect(ren, &sep);

    // Fields
    struct FieldDef { const char* lbl; int idx; bool pwd; };
    constexpr FieldDef fields[5] = {
        {"Name",     0, false},
        {"Host",     1, false},
        {"Port",     2, false},
        {"User",     3, false},
        {"Password", 4, true },
    };
    float fw = DW - 32;
    float ct = dy + 48;

    for (auto& f : fields) {
        float fy = ct + f.idx * FS_STEP;
        bool focused = (d.focus == f.idx);
        auto* val = dlg_field(d, f.idx);

        // label
        text_draw(ren, f.lbl, dx+16, fy + FS, C_DIM);

        // input box
        float by = fy + FS + 6;
        fill(ren, C_INBG, dx+16, by, fw, FH);
        rect(ren, focused ? C_ACCENT : C_BORDER, dx+16, by, fw, FH);

        // text (mask for password)
        std::string disp = f.pwd ? std::string(val->size(), '*') : *val;
        if (focused) disp += '|';
        // scroll right if overflowing
        while (disp.size() > 1 && text_w(disp.c_str()) > fw - 12)
            disp.erase(0, 1);
        text_draw(ren, disp.c_str(), dx+22, center_baseline(by, FH), C_TEXT);

        // click to focus
        if (click && hit(mx, my, dx+16, by, fw, FH))
            d.focus = f.idx;
    }

    // Error
    if (!d.err.empty())
        text_draw(ren, d.err.c_str(), dx+16, ct + 5*FS_STEP, C_ERR);

    // Buttons  ─  Save  |  Cancel
    float btn_y = dy + DH - 50;
    constexpr float BH = 30, BW_S = 90, BW_C = 80;
    float sx = dx + DW - 16 - BW_S;
    float cx2 = sx - 10 - BW_C;

    bool h_save = hit(mx, my, sx,  btn_y, BW_S, BH);
    bool h_can  = hit(mx, my, cx2, btn_y, BW_C, BH);

    fill(ren, h_save ? C_ACCENT : C_BORDER, sx,  btn_y, BW_S, BH);
    fill(ren, h_can  ? C_HOVER  : C_BORDER, cx2, btn_y, BW_C, BH);

    auto btn_text = [&](const char* t, float bx, float bw, Clr c) {
        float tw = text_w(t);
        text_draw(ren, t, bx + (bw - tw)*.5f, center_baseline(btn_y, BH), c);
    };
    btn_text("Save",   sx,  BW_S, h_save ? C_PANEL : C_TEXT);
    btn_text("Cancel", cx2, BW_C, C_TEXT);

    if (click) {
        if (h_can) return -1;
        if (h_save) {
            if (d.c.name.empty()) { d.err = "Name is required";      return 0; }
            if (d.c.host.empty()) { d.err = "Host is required";      return 0; }
            return 1;
        }
    }
    return 0;
}

// ── left panel ───────────────────────────────────────────────────────────────
static constexpr float ITEM_H  = 30.0f;
static constexpr float HDR_H   = 38.0f;
static constexpr float PAD     = 10.0f;
static constexpr float ICON_SZ = 8.0f;   // icon radius / half-size
static constexpr float EDIT_W  = 30.0f;  // width of edit button column

static void panel_render(SDL_Renderer* ren, App& app, bool click) {
    float pw = app.ww * 0.30f;
    float ph = (float)app.wh;

    fill(ren, C_PANEL, 0, 0, pw, ph);

    // ── header ────────────────────────────────────────────────────────────────
    fill(ren, C_BG, 0, 0, pw, HDR_H);
    text_draw(ren, "Connections", PAD, center_baseline(0, HDR_H), C_DIM);

    // add "+" button (right side of header)
    float abx = pw - HDR_H, aby = 0;
    bool h_add = hit(app.mx, app.my, abx, aby, HDR_H, HDR_H);
    app.h_add = h_add;
    if (h_add) fill(ren, C_HOVER, abx, aby, HDR_H, HDR_H);
    rect(ren, C_BORDER, abx, aby, HDR_H, HDR_H);
    draw_plus(ren, abx + HDR_H*.5f, HDR_H*.5f, ICON_SZ, h_add ? C_ACCENT : C_DIM);

    // separator
    sc(ren, C_BORDER);
    SDL_FRect sep{0, HDR_H-1, pw, 1};
    SDL_RenderFillRect(ren, &sep);

    if (click && h_add) {
        app.dlg = Dlg{};
        app.dlg.open = true;
        app.dlg.editing = false;
        SDL_StartTextInput(app.win);
    }

    // ── items ─────────────────────────────────────────────────────────────────
    app.h_item = -1;
    app.h_edit = -1;

    for (int i = 0; i < (int)app.conns.size(); i++) {
        float iy = HDR_H + i * ITEM_H;
        bool h_row  = hit(app.mx, app.my, 0,       iy, pw - EDIT_W, ITEM_H);
        bool h_edit = hit(app.mx, app.my, pw - EDIT_W, iy, EDIT_W,  ITEM_H);

        if (h_row)  app.h_item = i;
        if (h_edit) app.h_edit = i;

        // row background
        if (h_row || h_edit)
            fill(ren, C_HOVER, 0, iy, pw, ITEM_H);

        // name text
        const auto& name = app.conns[i].name;
        text_draw(ren, name.c_str(), PAD, center_baseline(iy, ITEM_H), C_TEXT);

        // edit pencil button
        Clr pencil_c = h_edit ? C_ACCENT : C_DIM;
        draw_pencil(ren, pw - EDIT_W*.5f, iy + ITEM_H*.5f, ICON_SZ, pencil_c);

        // item separator
        sc(ren, C_BORDER);
        SDL_FRect line{0, iy + ITEM_H - 1, pw, 1};
        SDL_RenderFillRect(ren, &line);

        // click on edit button
        if (click && h_edit) {
            app.dlg.open     = true;
            app.dlg.editing  = true;
            app.dlg.old_name = app.conns[i].name;
            app.dlg.c        = app.conns[i];
            app.dlg.focus    = 0;
            app.dlg.err      = "";
            SDL_StartTextInput(app.win);
        }
    }

    // right divider
    sc(ren, C_BORDER);
    SDL_FRect div{pw - 1, 0, 1, ph};
    SDL_RenderFillRect(ren, &div);
}

// ── main ──────────────────────────────────────────────────────────────────────
int main(int /*argc*/, char* /*argv*/[]) {
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return 1;
    }

    App app;
    app.win = SDL_CreateWindow("probe_lang", app.ww, app.wh, SDL_WINDOW_RESIZABLE);
    if (!app.win) { SDL_Log("CreateWindow: %s", SDL_GetError()); return 1; }

    app.ren = SDL_CreateRenderer(app.win, nullptr);
    if (!app.ren) { SDL_Log("CreateRenderer: %s", SDL_GetError()); return 1; }

    SDL_SetRenderDrawBlendMode(app.ren, SDL_BLENDMODE_NONE);
    font_init(app.ren);
    app.conns = load_all();

    // TEST: set PROBE_TEST_DIALOG=1 or PROBE_TEST_EDIT=1 to open dialog on startup
    if (SDL_GetEnvironmentVariable(SDL_GetEnvironment(), "PROBE_TEST_DIALOG")) {
        app.dlg.open = true; app.dlg.editing = false;
        SDL_StartTextInput(app.win);
    } else if (SDL_GetEnvironmentVariable(SDL_GetEnvironment(), "PROBE_TEST_EDIT") &&
               !app.conns.empty()) {
        app.dlg.open = true; app.dlg.editing = true;
        app.dlg.old_name = app.conns[0].name;
        app.dlg.c = app.conns[0];
        SDL_StartTextInput(app.win);
    }

    bool running = true;
    while (running) {
        bool click = false;
        float click_x = 0, click_y = 0;

        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            switch (ev.type) {
            case SDL_EVENT_QUIT:
                running = false;
                break;

            case SDL_EVENT_WINDOW_RESIZED:
                SDL_GetWindowSize(app.win, &app.ww, &app.wh);
                break;

            case SDL_EVENT_MOUSE_MOTION:
                app.mx = ev.motion.x;
                app.my = ev.motion.y;
                break;

            case SDL_EVENT_MOUSE_BUTTON_DOWN:
                if (ev.button.button == SDL_BUTTON_LEFT) {
                    click   = true;
                    click_x = ev.button.x;
                    click_y = ev.button.y;
                }
                break;

            case SDL_EVENT_TEXT_INPUT:
                if (app.dlg.open) {
                    auto* f = dlg_field(app.dlg, app.dlg.focus);
                    if (f) *f += ev.text.text;
                }
                break;

            case SDL_EVENT_KEY_DOWN:
                if (app.dlg.open) {
                    auto* f = dlg_field(app.dlg, app.dlg.focus);
                    switch (ev.key.key) {
                    case SDLK_BACKSPACE:
                        if (f && !f->empty()) f->pop_back();
                        break;
                    case SDLK_TAB:
                        app.dlg.focus = (app.dlg.focus + 1) % 5;
                        break;
                    case SDLK_ESCAPE:
                        app.dlg.open = false;
                        SDL_StopTextInput(app.win);
                        break;
                    case SDLK_RETURN: case SDLK_KP_ENTER:
                        if (app.dlg.focus < 4) {
                            app.dlg.focus++;
                        } else {
                            // attempt save
                            if (!app.dlg.c.name.empty() && !app.dlg.c.host.empty()) {
                                save_conn(app.dlg.c, app.dlg.old_name);
                                app.conns = load_all();
                                app.dlg.open = false;
                                SDL_StopTextInput(app.win);
                            } else {
                                app.dlg.err = app.dlg.c.name.empty()
                                    ? "Name is required" : "Host is required";
                            }
                        }
                        break;
                    default: break;
                    }
                }
                break;

            default: break;
            }
        }

        // ── render ────────────────────────────────────────────────────────────
        sc(app.ren, C_BG);
        SDL_RenderClear(app.ren);

        // left panel (handles hover + click itself)
        panel_render(app.ren, app, click);

        // right panel (stub)
        float pw = app.ww * 0.30f;
        fill(app.ren, C_BG, pw, 0, app.ww - pw, (float)app.wh);

        // dialog
        if (app.dlg.open) {
            int result = dlg_render(app.ren, app.dlg,
                                    click ? click_x : app.mx,
                                    click ? click_y : app.my,
                                    click);
            if (result == 1) {
                save_conn(app.dlg.c, app.dlg.old_name);
                app.conns = load_all();
                app.dlg.open = false;
                SDL_StopTextInput(app.win);
            } else if (result == -1) {
                app.dlg.open = false;
                SDL_StopTextInput(app.win);
            }
        }

        SDL_RenderPresent(app.ren);
        SDL_Delay(16); // ~60 fps
    }

    SDL_DestroyRenderer(app.ren);
    SDL_DestroyWindow(app.win);
    SDL_Quit();
    return 0;
}
