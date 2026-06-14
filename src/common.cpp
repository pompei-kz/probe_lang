#include "common.h"
#include "Conn.h"
#include "ContextMenu.h"
#include "FontAtlas.h"
#include "PanelMenu.h"
#include "render_helpers.h"

void draw_plus(SDL_Renderer *r, float cx, float cy, float sz, Clr c)
{
  sc(r, c);
  SDL_FRect h{cx - sz * .55f, cy - sz * .12f, sz * 1.1f, sz * .24f};
  SDL_FRect v{cx - sz * .12f, cy - sz * .55f, sz * .24f, sz * 1.1f};
  SDL_RenderFillRect(r, &h);
  SDL_RenderFillRect(r, &v);
}

void draw_pencil(SDL_Renderer *r, float cx, float cy, float sz, Clr c)
{
  sc(r, c);
  for (int i = -1; i <= 1; i++) {
    float ox = (float)i;
    SDL_RenderLine(r, cx - sz * .3f + ox, cy + sz * .4f, cx + sz * .3f + ox, cy - sz * .4f);
  }
  SDL_RenderLine(r, cx - sz * .3f - 1, cy + sz * .4f, cx - sz * .45f, cy + sz * .6f);
  SDL_RenderLine(r, cx - sz * .45f, cy + sz * .6f, cx - sz * .05f, cy + sz * .45f);
  SDL_FRect cap{cx + sz * .15f, cy - sz * .55f, sz * .25f, sz * .15f};
  SDL_RenderFillRect(r, &cap);
}

// ── dialog ────────────────────────────────────────────────────────────────────
static constexpr float DW = 440, DH = 400, FH = 28.f, FS_STEP = 58.f;

int dlg_render(SDL_Renderer *ren, Dlg &d, const DlgMouse &m)
{
  SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
  int ww, wh;
  SDL_GetCurrentRenderOutputSize(ren, &ww, &wh);
  fill(ren, C_OVL, 0, 0, (float)ww, (float)wh);
  SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_NONE);

  float dx = (ww - DW) * .5f, dy = (wh - DH) * .5f;
  fill(ren, C_DLGBG, dx, dy, DW, DH);
  rect(ren, C_BORDER, dx, dy, DW, DH);

  text_draw(ren, d.editing ? "Edit Connection" : "Add Connection", dx + 16, dy + 24, C_TEXT);
  sc(ren, C_BORDER);
  SDL_FRect sep{dx + 1, dy + 36, DW - 2, 1};
  SDL_RenderFillRect(ren, &sep);

  struct FieldDef
  {
    const char *lbl;
    int         idx;
  };
  constexpr FieldDef fields[5] = {{"Name", 0}, {"Host", 1}, {"Port", 2}, {"User", 3}, {"Password", 4}};
  float              fw = DW - 32, ct = dy + 48;
  float              text_ox = dx + 16.f + 6.f;

  for (auto &f : fields) {
    float fy      = ct + f.idx * FS_STEP;
    float by      = fy + FS + 6;
    bool  focused = (d.focus == f.idx);

    text_draw(ren, f.lbl, dx + 16, fy + FS, C_DIM);
    d.editors[f.idx].draw(ren, dx + 16, by, fw, FH, focused);

    if (m.ldown && !d.ctx_menu.open && hit(m.mx, m.my, dx + 16, by, fw, FH)) {
      d.focus    = f.idx;
      bool shift = (SDL_GetModState() & SDL_KMOD_SHIFT) != 0;
      d.editors[f.idx].on_mouse_press(text_ox, m.mx, m.clicks, shift);
      d.active_drag_ed = f.idx;
    }

    if (m.rdown && hit(m.mx, m.my, dx + 16, by, fw, FH)) {
      d.focus    = f.idx;
      float cx2  = std::min(m.mx, dx + DW - ContextMenu::W - 4.f);
      float cy2  = std::min(m.my, dy + DH - ContextMenu::N * ContextMenu::IH - 10.f);
      d.ctx_menu = ContextMenu{true, cx2, cy2, f.idx};
    }
  }

  if (!d.err.empty()) text_draw(ren, d.err.c_str(), dx + 16, ct + 5 * FS_STEP, C_ERR);

  float           btn_y = dy + DH - 50;
  constexpr float BH = 30, BW_S = 90, BW_C = 80;
  float           sx = dx + DW - 16 - BW_S, cx2 = sx - 10 - BW_C;

  bool h_save = hit(m.mx, m.my, sx, btn_y, BW_S, BH);
  bool h_can  = hit(m.mx, m.my, cx2, btn_y, BW_C, BH);
  fill(ren, h_save ? C_ACCENT : C_BORDER, sx, btn_y, BW_S, BH);
  fill(ren, h_can ? C_HOVER : C_BORDER, cx2, btn_y, BW_C, BH);

  auto btn_text = [&](const char *t, float bx, float bw, Clr c) { text_draw(ren, t, bx + (bw - text_w(t)) * .5f, center_baseline(btn_y, BH), c); };
  btn_text("Save", sx, BW_S, h_save ? C_PANEL : C_TEXT);
  btn_text("Cancel", cx2, BW_C, C_TEXT);

  int menu_act = d.ctx_menu.render(ren, m.mx, m.my, m.ldown, m.rdown);
  if (menu_act >= 0 && d.ctx_menu.ed_idx >= 0) {
    auto &ed = d.editors[d.ctx_menu.ed_idx];
    if (menu_act == 0)
      ed.do_copy();
    else if (menu_act == 1)
      ed.do_cut();
    else
      ed.do_paste();
  }

  if (m.ldown && !d.ctx_menu.open) {
    if (h_can) return -1;
    if (h_save) {
      Conn c = d.to_conn();
      if (c.name.empty()) {
        d.err = "Name is required";
        return 0;
      }
      if (c.host.empty()) {
        d.err = "Host is required";
        return 0;
      }
      return 1;
    }
  }
  return 0;
}

// ── left panel ────────────────────────────────────────────────────────────────
static constexpr float ITEM_H = 30.f, HDR_H = 38.f, PAD = 10.f, ICON_SZ = 8.f, EDIT_W = 30.f;

void panel_render(SDL_Renderer *ren, App &app, bool click, bool rclick)
{
  const float pw = app.ww * 0.30f;
  const float ph = (float)app.wh;

  fill(ren, C_PANEL, 0, 0, pw, ph);
  fill(ren, C_BG, 0, 0, pw, HDR_H);
  text_draw(ren, "Connections", PAD, center_baseline(0, HDR_H), C_DIM);

  float abx   = pw - HDR_H;
  bool  h_add = hit(app.mx, app.my, abx, 0, HDR_H, HDR_H);
  app.h_add   = h_add;
  if (h_add) fill(ren, C_HOVER, abx, 0, HDR_H, HDR_H);
  rect(ren, C_BORDER, abx, 0, HDR_H, HDR_H);
  draw_plus(ren, abx + HDR_H * .5f, HDR_H * .5f, ICON_SZ, h_add ? C_ACCENT : C_DIM);

  sc(ren, C_BORDER);
  SDL_FRect sep{0, HDR_H - 1, pw, 1};
  SDL_RenderFillRect(ren, &sep);

  if (click && h_add) {
    app.dlg.open_add();
    app.dlg.open = true;
    SDL_StartTextInput(app.win);
  }

  app.h_item = -1;
  app.h_edit = -1;
  for (int i = 0; i < (int)app.conns.size(); i++) {
    float iy     = HDR_H + i * ITEM_H;
    bool  h_row  = hit(app.mx, app.my, 0, iy, pw - EDIT_W, ITEM_H);
    bool  h_edit = hit(app.mx, app.my, pw - EDIT_W, iy, EDIT_W, ITEM_H);
    if (h_row) app.h_item = i;
    if (h_edit) app.h_edit = i;
    if (h_row || h_edit) fill(ren, C_HOVER, 0, iy, pw, ITEM_H);
    text_draw(ren, app.conns[i].name.c_str(), PAD, center_baseline(iy, ITEM_H), C_TEXT);
    draw_pencil(ren, pw - EDIT_W * .5f, iy + ITEM_H * .5f, ICON_SZ, h_edit ? C_ACCENT : C_DIM);
    sc(ren, C_BORDER);
    SDL_FRect line{0, iy + ITEM_H - 1, pw, 1};
    SDL_RenderFillRect(ren, &line);
    if (click && h_edit) {
      app.dlg.open_edit(app.conns[i]);
      app.dlg.open = true;
      SDL_StartTextInput(app.win);
    }
    if (rclick && (h_row || h_edit)) {
      float mx2 = std::min(app.mx, pw - PanelMenu::W - 2.f);
      float my2 = std::min(app.my, ph - PanelMenu::N * PanelMenu::IH - 10.f);
      app.panel_menu = PanelMenu{true, mx2, my2, i};
    }
  }

  sc(ren, C_BORDER);
  SDL_FRect div{pw - 1, 0, 1, ph};
  SDL_RenderFillRect(ren, &div);

  int act = app.panel_menu.render(ren, app.mx, app.my, click, rclick);
  if (act == 0) {
    app.dlg.open_add();
    app.dlg.open = true;
    SDL_StartTextInput(app.win);
  } else if (act == 1 && app.panel_menu.conn_idx >= 0 && app.panel_menu.conn_idx < (int)app.conns.size()) {
    app.dlg.open_edit(app.conns[app.panel_menu.conn_idx]);
    app.dlg.open = true;
    SDL_StartTextInput(app.win);
  } else if (act == 2 && app.panel_menu.conn_idx >= 0 && app.panel_menu.conn_idx < (int)app.conns.size()) {
    delete_conn(app.conns[app.panel_menu.conn_idx].name);
    app.conns = load_all();
  }
}
