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

  // Defaults for a freshly-created statement box (world units).
  static constexpr float DEF_W = 150.f;
  static constexpr float DEF_H = 44.f;
  static constexpr float ZOOM_MIN = 0.15f, ZOOM_MAX = 8.f;
  static constexpr float GRID = 50.f;

  // Screen-space geometry of a statement box, derived from its world rect.
  struct BoxGeo
  {
    float bx, by, bw, bh;       // outer box
    float badge_x, badge_y, badge; // square badge on the left
    float nx, ny, nw, nh;       // name / edit-field area
  };

  static BoxGeo box_geo(const EditorView &e, const Statement &s)
  {
    BoxGeo g{};
    g.bx = e.to_screen_x(s.x);
    g.by = e.to_screen_y(s.y);
    g.bw = static_cast<float>(s.width * e.zoom);
    g.bh = static_cast<float>(s.height * e.zoom);

    g.badge   = std::clamp(g.bh - 6.f, 8.f, 22.f);
    g.badge_x = g.bx + 5.f;
    g.badge_y = g.by + (g.bh - g.badge) * .5f;

    g.nx = g.badge_x + g.badge + 6.f;
    g.nh = std::min(g.bh - 6.f, FS + 8.f);
    g.ny = g.by + (g.bh - g.nh) * .5f;
    g.nw = std::max(g.bx + g.bw - g.nx - 6.f, 10.f);
    return g;
  }

  // Filled box with a left badge ("M"/"F") and a centred name.
  static void draw_box(SDL_Renderer *ren, const BoxGeo &g, char letter, const char *name, bool hovered)
  {
    fill(ren, hovered ? C_HOVER : C_PANEL, g.bx, g.by, g.bw, g.bh);
    rect(ren, C_BORDER, g.bx, g.by, g.bw, g.bh);

    fill(ren, C_ACCENT, g.badge_x, g.badge_y, g.badge, g.badge);
    const char ls[2] = {letter, '\0'};
    text_draw(ren, ls, g.badge_x + (g.badge - text_w(ls)) * .5f, center_baseline(g.badge_y, g.badge), C_PANEL);

    if (name && *name) text_draw(ren, name, g.nx, center_baseline(g.by, g.bh), C_TEXT);
  }

  void EditorView::open_for(const Conn &c, const std::string &schema_, const std::string &uid, const std::string &uname)
  {
    open         = true;
    conn         = c;
    schema       = schema_;
    unit_id      = uid;
    unit_name    = uname;
    zoom         = 1.0;
    cam_init     = false;
    last_pw      = last_ph = -1;
    panning      = panned = false;
    chooser_open = false;
    editing      = false;
    stmts.clear();
  }

  void EditorView::close()
  {
    open         = false;
    editing      = false;
    chooser_open = false;
    panning      = false;
  }

  void EditorView::init_camera()
  {
    zoom = 1.0;
    auto [box, err] = statement_bbox_for_unit(conn, schema, unit_id);
    if (box) {
      const double cx = (box->min_x + box->max_x) * .5;
      const double cy = (box->min_y + box->max_y) * .5;
      cam_x           = cx - (pw * .5) / zoom;
      cam_y           = cy - (ph * .5) / zoom;
    } else {
      // No statements yet: centre the world origin in the pane.
      cam_x = -(pw * .5) / zoom;
      cam_y = -(ph * .5) / zoom;
    }
    cam_init = true;
    last_pw  = pw;
    last_ph  = ph;
    reload();
  }

  void EditorView::reload()
  {
    const float minx = static_cast<float>(cam_x);
    const float miny = static_cast<float>(cam_y);
    const float maxx = static_cast<float>(cam_x + pw / zoom);
    const float maxy = static_cast<float>(cam_y + ph / zoom);
    auto [rows, err] = load_statements_in_view(conn, schema, minx, miny, maxx, maxy);
    stmts            = std::move(rows);
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
    update_statement_name(conn, schema, edit_id, edit_type, edit_field.ed.buf);
    editing = false;
    reload();
  }

  void EditorView::on_wheel(float dy, float mx, float my)
  {
    if (!open || editing || chooser_open) return;
    if (!hit(mx, my, px, py, pw, ph)) return;

    const double wx     = to_world_x(mx);
    const double wy     = to_world_y(my);
    const double factor = dy > 0 ? 1.15 : 1.0 / 1.15;
    zoom                = std::clamp(zoom * factor, static_cast<double>(ZOOM_MIN), static_cast<double>(ZOOM_MAX));
    // Keep the world point under the cursor fixed.
    cam_x = wx - (mx - px) / zoom;
    cam_y = wy - (my - py) / zoom;
    reload();
  }

  void EditorView::on_mouse_move(float mx, float my)
  {
    if (!open) return;
    if (editing) edit_field.on_move(mx);
    if (panning) {
      const float dx = mx - pan_last_x;
      const float dy = my - pan_last_y;
      if (dx != 0 || dy != 0) {
        cam_x -= dx / zoom;
        cam_y -= dy / zoom;
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

  // ── chooser popup ──────────────────────────────────────────────────────────
  // Returns true if it consumed the click (selection made or dismissed).
  static bool draw_chooser(EditorView &e, SDL_Renderer *ren, float mx, float my, bool ldown)
  {
    constexpr float PAD = 8.f, SW = 180.f, SH = 40.f, GAP = 8.f;
    const float     panel_w = SW + 2 * PAD;
    const float     panel_h = 2 * SH + GAP + 2 * PAD;

    float ox = std::min(e.chooser_sx, e.px + e.pw - panel_w);
    float oy = std::min(e.chooser_sy, e.py + e.ph - panel_h);
    ox       = std::max(ox, e.px);
    oy       = std::max(oy, e.py);

    fill(ren, C_DLGBG, ox, oy, panel_w, panel_h);
    rect(ren, C_BORDER, ox, oy, panel_w, panel_h);

    auto sample = [&](float sy, char letter, const char *name) -> bool {
      BoxGeo g{};
      g.bx = ox + PAD;
      g.by = sy;
      g.bw = SW;
      g.bh = SH;
      g.badge   = std::clamp(g.bh - 6.f, 8.f, 22.f);
      g.badge_x = g.bx + 5.f;
      g.badge_y = g.by + (g.bh - g.badge) * .5f;
      g.nx      = g.badge_x + g.badge + 6.f;
      bool hov  = hit(mx, my, g.bx, g.by, g.bw, g.bh);
      draw_box(ren, g, letter, name, hov);
      return hov;
    };

    bool hm = sample(oy + PAD, 'M', "newMethod");
    bool hf = sample(oy + PAD + SH + GAP, 'F', "newField");

    if (ldown) {
      if (hm) {
        create_statement(e.conn, e.schema, e.unit_id, StatementType::Method, e.chooser_wx, e.chooser_wy, DEF_W, DEF_H, "newMethod");
        e.chooser_open = false;
        e.reload();
      } else if (hf) {
        create_statement(e.conn, e.schema, e.unit_id, StatementType::Field, e.chooser_wx, e.chooser_wy, DEF_W, DEF_H, "newField");
        e.chooser_open = false;
        e.reload();
      } else {
        e.chooser_open = false; // click outside dismisses
      }
      return true;
    }
    return false;
  }

  void EditorView::render(SDL_Renderer *ren, float pane_x, float pane_y, float pane_w, float pane_h, float mx, float my, bool ldown, bool rdown, int clicks)
  {
    if (!open) return;
    px = pane_x;
    py = pane_y;
    pw = pane_w;
    ph = pane_h;

    if (!cam_init)
      init_camera();
    else if (pw != last_pw || ph != last_ph) {
      last_pw = pw;
      last_ph = ph;
      reload();
    }

    SDL_Rect clip{static_cast<int>(px), static_cast<int>(py), static_cast<int>(pw), static_cast<int>(ph)};
    SDL_SetRenderClipRect(ren, &clip);

    // ── grid ────────────────────────────────────────────────────────────────
    const float step = static_cast<float>(GRID * zoom);
    if (step >= 6.f) {
      sc(ren, C_PANEL);
      double first_x = std::floor(cam_x / GRID) * GRID;
      for (double wx = first_x; to_screen_x(wx) < px + pw; wx += GRID) {
        float sx = to_screen_x(wx);
        if (sx >= px) SDL_RenderLine(ren, sx, py, sx, py + ph);
      }
      double first_y = std::floor(cam_y / GRID) * GRID;
      for (double wy = first_y; to_screen_y(wy) < py + ph; wy += GRID) {
        float sy = to_screen_y(wy);
        if (sy >= py) SDL_RenderLine(ren, px, sy, px + pw, sy);
      }
    }

    // ── statement boxes ───────────────────────────────────────────────────────
    for (const Statement &s : stmts) {
      BoxGeo     g   = box_geo(*this, s);
      const bool hov = !editing && !chooser_open && hit(mx, my, g.bx, g.by, g.bw, g.bh);
      const char letter = s.type == StatementType::Field ? 'F' : 'M';

      if (editing && s.id == edit_id) {
        // Draw the box frame + badge, then an editable field over the name area.
        draw_box(ren, g, letter, "", false);
        edit_bx = g.nx;
        edit_bw = g.nw;
        edit_bh = std::min(g.bh - 6.f, FS + 8.f);
        edit_by = g.by + (g.bh - edit_bh) * .5f;
        edit_field.draw(ren, edit_bx, edit_by, edit_bw, edit_bh, true);
      } else {
        draw_box(ren, g, letter, s.name.c_str(), hov);
      }
    }

    // ── unit name hint ────────────────────────────────────────────────────────
    if (!unit_name.empty()) text_draw(ren, unit_name.c_str(), px + 8.f, py + 8.f + FS, C_DIM);

    const bool dbl = ldown && clicks >= 2;

    // ── inline editing mode consumes all interaction ──────────────────────────
    if (editing) {
      const float text_ox = edit_bx + 6.f;
      if (ldown) {
        if (hit(mx, my, edit_bx, edit_by, edit_bw, edit_bh))
          edit_field.on_ldown(text_ox, mx, my, edit_bx, edit_by, edit_bw, edit_bh, clicks);
        else
          commit_edit();
      }
      SDL_SetRenderClipRect(ren, nullptr);
      return;
    }

    // ── chooser popup ─────────────────────────────────────────────────────────
    if (chooser_open) {
      draw_chooser(*this, ren, mx, my, ldown);
      SDL_SetRenderClipRect(ren, nullptr);
      return;
    }

    // ── double-click: edit a box's name, or open the chooser on empty space ────
    // Guard to the pane: the double-click that opened the editor lands in the
    // tree (left pane) and must not be treated as an empty-canvas double-click.
    if (dbl && hit(mx, my, px, py, pw, ph)) {
      const Statement *target = nullptr;
      BoxGeo           tgeo{};
      for (const Statement &s : stmts) { // last hit wins (topmost)
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

    // ── single click begins a pan ─────────────────────────────────────────────
    if (ldown && hit(mx, my, px, py, pw, ph)) {
      panning    = true;
      panned     = false;
      pan_last_x = mx;
      pan_last_y = my;
    }

    SDL_SetRenderClipRect(ren, nullptr);
  }

} // namespace front
