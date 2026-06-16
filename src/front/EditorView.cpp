#include "EditorView.h"
#include "Clr.h"
#include "FontAtlas.h"
#include "back/StatementService.h"
#include "render_helpers.h"

#include <algorithm>
#include <cmath>

namespace front {

  using namespace back;
  using namespace back::model;

  static constexpr float DEF_W = 150.f;
  static constexpr float DEF_H = 44.f;
  static constexpr float ZOOM_MIN = 0.15f, ZOOM_MAX = 8.f;
  static constexpr float GRID  = 50.f;
  static constexpr float TAB_H = 30.f;

  // ── unit-type glyph (mirrors the tree's draw_unit_icon) ──────────────────────
  static void draw_unit_icon(SDL_Renderer *r, float x, float yc, UnitType t, Clr c)
  {
    sc(r, c);
    switch (t) {
      case UnitType::Class: {
        SDL_FRect b{x, yc - 4.f, 9.f, 9.f};
        SDL_RenderFillRect(r, &b);
        break;
      }
      case UnitType::Interface: {
        SDL_FRect b{x, yc - 4.f, 9.f, 9.f};
        SDL_RenderRect(r, &b);
        break;
      }
      case UnitType::Enum: {
        for (int i = 0; i < 3; i++) {
          SDL_FRect bar{x, yc - 4.f + i * 3.5f, 9.f, 1.6f};
          SDL_RenderFillRect(r, &bar);
        }
        break;
      }
    }
  }

  static float tab_width(const EditorTab &t)
  {
    return std::clamp(10.f + 14.f + text_w(t.unit_name.c_str()) + 10.f, 90.f, 240.f);
  }

  // ── statement box geometry / drawing ─────────────────────────────────────────
  struct BoxGeo
  {
    float bx, by, bw, bh;
    float badge_x, badge_y, badge;
    float nx, ny, nw, nh;
  };

  // Layout is computed at scale 1 (world/unzoomed) and multiplied by the zoom,
  // so the box and everything inside it scale together.
  static BoxGeo box_geo(const EditorView &e, const Statement &s)
  {
    const float z = static_cast<float>(e.cur() ? e.cur()->zoom : 1.0);
    BoxGeo      g{};
    g.bx = e.to_screen_x(s.x);
    g.by = e.to_screen_y(s.y);
    g.bw = s.width * z;
    g.bh = s.height * z;

    const float badge0 = std::clamp(s.height - 6.f, 8.f, 22.f); // unscaled badge
    g.badge            = badge0 * z;
    g.badge_x          = g.bx + 5.f * z;
    g.badge_y          = g.by + (g.bh - g.badge) * .5f;

    g.nx = g.bx + (5.f + badge0 + 6.f) * z;
    g.nh = std::min(g.bh - 6.f, FS + 8.f);
    g.ny = g.by + (g.bh - g.nh) * .5f;
    g.nw = std::max(g.bx + g.bw - g.nx - 6.f * z, 10.f);
    return g;
  }

  // `scale` scales the text (name + badge letter) with the box; 1 == unzoomed.
  // Unscaled (world-unit) box width needed to fit `name`: matches box_geo's
  // horizontal layout (left pad + badge + gap + text + right pad).
  static float fit_width(float height, const std::string &name)
  {
    const float badge0 = std::clamp(height - 6.f, 8.f, 22.f);
    const float w      = 5.f + badge0 + 6.f + text_w(name.c_str()) + 6.f;
    return std::max(w, 60.f);
  }

  static void draw_box(SDL_Renderer *ren, const BoxGeo &g, char letter, const char *name, bool hovered, float scale)
  {
    fill(ren, hovered ? C_HOVER : C_PANEL, g.bx, g.by, g.bw, g.bh);
    rect(ren, C_BORDER, g.bx, g.by, g.bw, g.bh);

    fill(ren, C_ACCENT, g.badge_x, g.badge_y, g.badge, g.badge);
    const char ls[2] = {letter, '\0'};
    text_draw_scaled(ren, ls, g.badge_x + (g.badge - text_w(ls) * scale) * .5f, center_baseline_scaled(g.badge_y, g.badge, scale), C_PANEL, scale);

    if (name && *name) text_draw_scaled(ren, name, g.nx, center_baseline_scaled(g.by, g.bh, scale), C_TEXT, scale);
  }

  // ── lifecycle ────────────────────────────────────────────────────────────────
  void EditorView::open_for(const Conn &c, const std::string &schema_, const std::string &uid, const std::string &uname, UnitType utype)
  {
    for (int i = 0; i < static_cast<int>(tabs.size()); i++) {
      if (tabs[i].unit_id == uid && tabs[i].schema == schema_ && tabs[i].conn.name == c.name) {
        active = i;
        open   = true;
        return;
      }
    }
    EditorTab t{};
    t.conn      = c;
    t.schema    = schema_;
    t.unit_id   = uid;
    t.unit_name = uname;
    t.unit_type = utype;
    tabs.push_back(std::move(t));
    active       = static_cast<int>(tabs.size()) - 1;
    open         = true;
    editing      = false;
    chooser_open = false;
    panning      = false;
  }

  void EditorView::close()
  {
    open         = false;
    editing      = false;
    chooser_open = false;
    panning      = false;
  }

  void EditorView::close_tab(int i)
  {
    if (i < 0 || i >= static_cast<int>(tabs.size())) return;
    tabs.erase(tabs.begin() + i);
    editing      = false;
    chooser_open = false;
    panning      = false;
    if (tabs.empty()) {
      active = -1;
      open   = false;
      return;
    }
    if (active > i)
      active--;
    else if (active >= static_cast<int>(tabs.size()))
      active = static_cast<int>(tabs.size()) - 1;
  }

  int EditorView::tab_at(float mx, float my) const
  {
    if (my < py || my >= py + TAB_H) return -1;
    float x = px;
    for (int i = 0; i < static_cast<int>(tabs.size()); i++) {
      float w = tab_width(tabs[i]);
      if (mx >= x && mx < x + w) return i;
      x += w;
    }
    return -1;
  }

  void EditorView::init_camera()
  {
    EditorTab *t = cur();
    if (!t) return;
    t->zoom         = 1.0;
    auto [box, err] = statement_bbox_for_unit(t->conn, t->schema, t->unit_id);
    if (box) {
      const double mx = (box->min_x + box->max_x) * .5;
      const double my = (box->min_y + box->max_y) * .5;
      t->cam_x        = mx - (cw * .5) / t->zoom;
      t->cam_y        = my - (ch * .5) / t->zoom;
    } else {
      t->cam_x = -(cw * .5) / t->zoom;
      t->cam_y = -(ch * .5) / t->zoom;
    }
    t->cam_init = true;
    t->last_cw  = cw;
    t->last_ch  = ch;
    reload();
  }

  void EditorView::reload()
  {
    EditorTab *t = cur();
    if (!t) return;
    const float minx = static_cast<float>(t->cam_x);
    const float miny = static_cast<float>(t->cam_y);
    const float maxx = static_cast<float>(t->cam_x + cw / t->zoom);
    const float maxy = static_cast<float>(t->cam_y + ch / t->zoom);
    auto [rows, err] = load_statements_in_view(t->conn, t->schema, t->unit_id, minx, miny, maxx, maxy);
    t->stmts         = std::move(rows);
  }

  void EditorView::start_edit(const Statement &s, float fbx, float fby, float fbw, float fbh)
  {
    editing             = true;
    edit_id             = s.id;
    edit_type           = s.type;
    edit_field.ed       = TextEditor{};
    edit_field.ed.set(s.name);
    edit_field.ctx.open = false;
    edit_bx = fbx;
    edit_by = fby;
    edit_bw = fbw;
    edit_bh = fbh;
  }

  void EditorView::commit_edit()
  {
    if (!editing) return;
    EditorTab *t = cur();
    if (t) {
      const std::string &name = edit_field.ed.buf;
      update_statement_name(t->conn, t->schema, edit_id, edit_type, name);

      // Recompute the box width to fit the new name (height unchanged) and persist it.
      float height = DEF_H;
      for (const Statement &s : t->stmts)
        if (s.id == edit_id) {
          height = s.height;
          break;
        }
      update_statement_size(t->conn, t->schema, edit_id, fit_width(height, name), height);
    }
    editing = false;
    reload();
  }

  // ── event hooks ──────────────────────────────────────────────────────────────
  void EditorView::on_wheel(float dy, float mx, float my)
  {
    if (!open || editing || chooser_open) return;
    EditorTab *t = cur();
    if (!t || !hit(mx, my, cx, cy, cw, ch)) return;

    const double wx     = to_world_x(mx);
    const double wy     = to_world_y(my);
    const double factor = dy > 0 ? 1.15 : 1.0 / 1.15;
    t->zoom             = std::clamp(t->zoom * factor, static_cast<double>(ZOOM_MIN), static_cast<double>(ZOOM_MAX));
    t->cam_x            = wx - (mx - cx) / t->zoom;
    t->cam_y            = wy - (my - cy) / t->zoom;
    reload();
  }

  void EditorView::on_mouse_move(float mx, float my)
  {
    if (!open) return;
    if (editing) edit_field.on_move(mx);
    EditorTab *t = cur();
    if (panning && t) {
      const float dx = mx - pan_last_x;
      const float dy = my - pan_last_y;
      if (dx != 0 || dy != 0) {
        t->cam_x -= dx / t->zoom;
        t->cam_y -= dy / t->zoom;
        panned = true;
      }
      pan_last_x = mx;
      pan_last_y = my;
    }
  }

  void EditorView::on_mouse_up()
  {
    if (!open) return;
    if (editing) edit_field.on_release();
    if (panning) {
      panning = false;
      if (panned) reload();
      panned = false;
    }
  }

  void EditorView::on_middle_down(float mx, float my)
  {
    if (!open) return;
    int i = tab_at(mx, my);
    if (i >= 0) close_tab(i);
  }

  bool EditorView::handle_key(SDL_Keycode key, SDL_Keymod mod)
  {
    if (!open || !editing) return false;
    if (key == SDLK_RETURN || key == SDLK_KP_ENTER) {
      commit_edit();
      return true;
    }
    if (key == SDLK_ESCAPE) {
      editing = false; // cancel without saving
      return true;
    }
    return edit_field.handle_key(key, mod);
  }

  void EditorView::handle_text(const char *t)
  {
    if (open && editing) edit_field.handle_text(t);
  }

  // ── chooser popup ────────────────────────────────────────────────────────────
  static void draw_chooser(EditorView &e, SDL_Renderer *ren, float mx, float my, bool ldown)
  {
    EditorTab *t = e.cur();
    if (!t) return;

    constexpr float PAD = 8.f, SW = 180.f, SH = 40.f, GAP = 8.f;
    const float     panel_w = SW + 2 * PAD;
    const float     panel_h = 2 * SH + GAP + 2 * PAD;

    float ox = std::clamp(e.chooser_sx, e.cx, e.cx + e.cw - panel_w);
    float oy = std::clamp(e.chooser_sy, e.cy, e.cy + e.ch - panel_h);

    fill(ren, C_DLGBG, ox, oy, panel_w, panel_h);
    rect(ren, C_BORDER, ox, oy, panel_w, panel_h);

    auto sample = [&](float sy, char letter, const char *name) -> bool {
      BoxGeo g{};
      g.bx      = ox + PAD;
      g.by      = sy;
      g.bw      = SW;
      g.bh      = SH;
      g.badge   = std::clamp(g.bh - 6.f, 8.f, 22.f);
      g.badge_x = g.bx + 5.f;
      g.badge_y = g.by + (g.bh - g.badge) * .5f;
      g.nx      = g.badge_x + g.badge + 6.f;
      bool hov  = hit(mx, my, g.bx, g.by, g.bw, g.bh);
      draw_box(ren, g, letter, name, hov, 1.f);
      return hov;
    };

    bool hm = sample(oy + PAD, 'M', "newMethod");
    bool hf = sample(oy + PAD + SH + GAP, 'F', "newField");

    if (ldown) {
      if (hm) {
        create_statement(t->conn, t->schema, t->unit_id, StatementType::Method, e.chooser_wx, e.chooser_wy, DEF_W, DEF_H, "newMethod");
        e.chooser_open = false;
        e.reload();
      } else if (hf) {
        create_statement(t->conn, t->schema, t->unit_id, StatementType::Field, e.chooser_wx, e.chooser_wy, DEF_W, DEF_H, "newField");
        e.chooser_open = false;
        e.reload();
      } else {
        e.chooser_open = false;
      }
    }
  }

  // ── tab strip ────────────────────────────────────────────────────────────────
  void EditorView::render(SDL_Renderer *ren, float pane_x, float pane_y, float pane_w, float pane_h, float mx, float my, bool ldown, bool rdown, int clicks)
  {
    if (!open) return;
    if (active < 0 || tabs.empty()) {
      open = false;
      return;
    }
    px = pane_x;
    py = pane_y;
    pw = pane_w;
    ph = pane_h;
    cx = px;
    cy = py + TAB_H;
    cw = pw;
    ch = ph - TAB_H;

    const bool dbl         = ldown && clicks >= 2;
    const int  hovered_tab = tab_at(mx, my);

    // Tab strip.
    fill(ren, C_BG, px, py, pw, TAB_H);
    float tx = px;
    for (int i = 0; i < static_cast<int>(tabs.size()); i++) {
      const float tw  = tab_width(tabs[i]);
      const bool  act = i == active;
      fill(ren, act ? C_PANEL : (hovered_tab == i ? C_HOVER : C_BG), tx, py, tw, TAB_H);
      fill(ren, C_BORDER, tx + tw - 1, py, 1, TAB_H);
      draw_unit_icon(ren, tx + 10, py + TAB_H * .5f, tabs[i].unit_type, act ? C_ACCENT : C_DIM);
      text_draw(ren, tabs[i].unit_name.c_str(), tx + 24, center_baseline(py, TAB_H), act ? C_TEXT : C_DIM);
      if (act) fill(ren, C_ACCENT, tx, py + TAB_H - 2, tw, 2);
      tx += tw;
    }
    fill(ren, C_BORDER, px, py + TAB_H - 1, pw, 1);

    // Left-click selects a tab.
    bool tab_clicked = false;
    if (ldown && hovered_tab >= 0) {
      if (editing) commit_edit();
      if (hovered_tab != active) {
        active       = hovered_tab;
        chooser_open = false;
        panning      = false;
      }
      tab_clicked = true;
    }

    EditorTab *t = cur();
    if (!t) return;

    if (!t->cam_init)
      init_camera();
    else if (cw != t->last_cw || ch != t->last_ch) {
      t->last_cw = cw;
      t->last_ch = ch;
      reload();
    }

    // Canvas.
    SDL_Rect clip{static_cast<int>(cx), static_cast<int>(cy), static_cast<int>(cw), static_cast<int>(ch)};
    SDL_SetRenderClipRect(ren, &clip);

    // Grid.
    const float step = static_cast<float>(GRID * t->zoom);
    if (step >= 6.f) {
      sc(ren, C_PANEL);
      double first_x = std::floor(t->cam_x / GRID) * GRID;
      for (double wx = first_x; to_screen_x(wx) < cx + cw; wx += GRID) {
        float sx = to_screen_x(wx);
        if (sx >= cx) SDL_RenderLine(ren, sx, cy, sx, cy + ch);
      }
      double first_y = std::floor(t->cam_y / GRID) * GRID;
      for (double wy = first_y; to_screen_y(wy) < cy + ch; wy += GRID) {
        float sy = to_screen_y(wy);
        if (sy >= cy) SDL_RenderLine(ren, cx, sy, cx + cw, sy);
      }
    }

    // Statement boxes.
    for (const Statement &s : t->stmts) {
      BoxGeo     g      = box_geo(*this, s);
      const bool hov    = !editing && !chooser_open && !tab_clicked && hit(mx, my, g.bx, g.by, g.bw, g.bh);
      const char letter = s.type == StatementType::Field ? 'F' : 'M';

      if (editing && s.id == edit_id) {
        draw_box(ren, g, letter, "", false, static_cast<float>(t->zoom));
        edit_bx = g.nx;
        edit_bw = g.nw;
        edit_bh = std::min(g.bh - 6.f, FS + 8.f);
        edit_by = g.by + (g.bh - edit_bh) * .5f;
        edit_field.draw(ren, edit_bx, edit_by, edit_bw, edit_bh, true);
      } else {
        draw_box(ren, g, letter, s.name.c_str(), hov, static_cast<float>(t->zoom));
      }
    }

    // Inline editing consumes all canvas interaction.
    if (editing) {
      const float text_ox = edit_bx + 6.f;
      if (ldown && !tab_clicked) {
        if (hit(mx, my, edit_bx, edit_by, edit_bw, edit_bh))
          edit_field.on_ldown(text_ox, mx, my, edit_bx, edit_by, edit_bw, edit_bh, clicks);
        else
          commit_edit();
      }
      SDL_SetRenderClipRect(ren, nullptr);
      return;
    }

    if (chooser_open) {
      draw_chooser(*this, ren, mx, my, ldown && !tab_clicked);
      SDL_SetRenderClipRect(ren, nullptr);
      return;
    }

    if (tab_clicked) {
      SDL_SetRenderClipRect(ren, nullptr);
      return;
    }

    // Double-click: edit a box's name, or open the chooser on empty canvas.
    if (dbl && hit(mx, my, cx, cy, cw, ch)) {
      const Statement *target = nullptr;
      BoxGeo           tgeo{};
      for (const Statement &s : t->stmts) {
        BoxGeo g = box_geo(*this, s);
        if (hit(mx, my, g.bx, g.by, g.bw, g.bh)) {
          target = &s;
          tgeo   = g;
        }
      }
      if (target) {
        float fbh = std::min(tgeo.bh - 6.f, FS + 8.f);
        float fby = tgeo.by + (tgeo.bh - fbh) * .5f;
        start_edit(*target, tgeo.nx, fby, tgeo.nw, fbh);
      } else {
        chooser_open = true;
        chooser_wx   = static_cast<float>(to_world_x(mx));
        chooser_wy   = static_cast<float>(to_world_y(my));
        chooser_sx   = mx;
        chooser_sy   = my;
      }
      SDL_SetRenderClipRect(ren, nullptr);
      return;
    }

    // Single click on the canvas begins a pan.
    if (ldown && hit(mx, my, cx, cy, cw, ch)) {
      panning    = true;
      panned     = false;
      pan_last_x = mx;
      pan_last_y = my;
    }

    SDL_SetRenderClipRect(ren, nullptr);
  }

} // namespace front
