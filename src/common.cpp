#include "common.h"
#include "Conn.h"
#include "ConnNode.h"
#include "ConnTest.h"
#include "ContextMenu.h"
#include "FontAtlas.h"
#include "PanelMenu.h"
#include "SchemaNode.h"
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
static constexpr float DW = 440, DH = 560, FH = 28.f, FS_STEP = 58.f;

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
  constexpr FieldDef fields[6] = {{"Name", 0}, {"Host", 1}, {"Port", 2}, {"DB Name", 3}, {"User", 4}, {"Password", 5}};
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

  // ── Test Connection button ────────────────────────────────────────────────
  constexpr float BH = 30.f, BW_T = 170.f, BW_S = 90.f, BW_C = 80.f;
  float           test_btn_y = ct + 6 * FS_STEP + 2;
  bool            h_test     = hit(m.mx, m.my, dx + 16, test_btn_y, BW_T, BH);
  fill(ren, h_test ? C_HOVER : C_BORDER, dx + 16, test_btn_y, BW_T, BH);
  text_draw(ren, "Test connection",
            dx + 16 + (BW_T - text_w("Test connection")) * .5f,
            center_baseline(test_btn_y, BH), C_TEXT);

  if (m.ldown && h_test && !d.ctx_menu.open) {
    d.err = "";
    auto [ok, msg]  = test_connection(d.editors[1].buf, d.editors[2].buf,
                                      d.editors[3].buf, d.editors[4].buf,
                                      d.editors[5].buf);
    d.test_ok  = ok;
    d.test_msg = msg;
    if (ok) {
      d.snap_host   = d.editors[1].buf;
      d.snap_port   = d.editors[2].buf;
      d.snap_dbname = d.editors[3].buf;
      d.snap_user   = d.editors[4].buf;
      d.snap_pass   = d.editors[5].buf;
    }
  }

  // test result message
  if (!d.test_msg.empty())
    text_draw(ren, d.test_msg.c_str(), dx + 16, test_btn_y + 38, d.test_ok ? C_OK : C_ERR);

  // validation error
  if (!d.err.empty())
    text_draw(ren, d.err.c_str(), dx + 16, test_btn_y + 60, C_ERR);

  // ── Save / Cancel buttons ─────────────────────────────────────────────────
  float btn_y    = dy + DH - 52;
  bool  can_save = d.save_enabled();
  float sx       = dx + DW - 16 - BW_S;
  float cx2      = sx - 10 - BW_C;

  bool h_save = can_save && hit(m.mx, m.my, sx, btn_y, BW_S, BH);
  bool h_can  = hit(m.mx, m.my, cx2, btn_y, BW_C, BH);
  fill(ren, can_save ? (h_save ? C_ACCENT : C_BORDER) : C_DIM, sx, btn_y, BW_S, BH);
  fill(ren, h_can ? C_HOVER : C_BORDER, cx2, btn_y, BW_C, BH);

  auto btn_text = [&](const char *t, float bx, float bw, Clr c) {
    text_draw(ren, t, bx + (bw - text_w(t)) * .5f, center_baseline(btn_y, BH), c);
  };
  btn_text("Save",   sx,  BW_S, can_save ? (h_save ? C_PANEL : C_TEXT) : C_DIM);
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
    if (h_save && can_save) {
      Conn c = d.to_conn();
      if (c.name.empty()) { d.err = "Name is required"; return 0; }
      return 1;
    }
  }
  return 0;
}

// ── left panel helpers ────────────────────────────────────────────────────────
static constexpr float ITEM_H  = 30.f, HDR_H  = 38.f, PAD    = 10.f;
static constexpr float ICON_SZ = 8.f,  EDIT_W = 30.f, CARET_W = 22.f;

// Right-pointing (collapsed) or down-pointing (expanded) triangle
static void draw_caret(SDL_Renderer *r, float cx, float cy, bool open, Clr c)
{
  sc(r, c);
  if (open) {
    // ▼
    SDL_RenderLine(r, cx - 5, cy - 2, cx + 5, cy - 2);
    SDL_RenderLine(r, cx - 5, cy - 2, cx,     cy + 3);
    SDL_RenderLine(r, cx + 5, cy - 2, cx,     cy + 3);
    SDL_RenderLine(r, cx - 3, cy,     cx + 3, cy);
  } else {
    // ▶
    SDL_RenderLine(r, cx - 2, cy - 5, cx - 2, cy + 5);
    SDL_RenderLine(r, cx - 2, cy - 5, cx + 3, cy);
    SDL_RenderLine(r, cx - 2, cy + 5, cx + 3, cy);
    SDL_RenderLine(r, cx - 2, cy,     cx,     cy);
  }
}

void panel_render(SDL_Renderer *ren, App &app, bool click, bool rclick, bool dblclick)
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
  int row    = 0;

  for (int i = 0; i < (int)app.conns.size(); i++) {
    ConnNode   &node      = app.conns[i];
    const bool  connected = node.conn.connected;
    float       iy        = HDR_H + row * ITEM_H;

    bool h_caret = connected && hit(app.mx, app.my, 0, iy, PAD + CARET_W, ITEM_H);
    bool h_row   = !h_caret && hit(app.mx, app.my, 0, iy, pw - EDIT_W, ITEM_H);
    bool h_edit  = hit(app.mx, app.my, pw - EDIT_W, iy, EDIT_W, ITEM_H);
    if (h_row || h_caret) app.h_item = i;
    if (h_edit)           app.h_edit = i;
    if (h_row || h_caret || h_edit) fill(ren, C_HOVER, 0, iy, pw, ITEM_H);

    // caret on the left (only for connected nodes)
    float caret_cx = PAD + CARET_W * .5f;
    float caret_cy = iy + ITEM_H * .5f;
    if (connected)
      draw_caret(ren, caret_cx, caret_cy, node.open, node.open ? C_ACCENT : C_DIM);

    // name: indented to make room for caret; dim if disconnected
    Clr name_clr = connected ? C_TEXT : C_DIM;
    text_draw(ren, node.conn.name.c_str(), PAD + CARET_W, center_baseline(iy, ITEM_H), name_clr);

    draw_pencil(ren, pw - EDIT_W * .5f, iy + ITEM_H * .5f, ICON_SZ, h_edit ? C_ACCENT : C_DIM);

    sc(ren, C_BORDER);
    SDL_FRect line{0, iy + ITEM_H - 1, pw, 1};
    SDL_RenderFillRect(ren, &line);

    if (click && h_edit) {
      app.dlg.open_edit(node.conn);
      app.dlg.open = true;
      SDL_StartTextInput(app.win);
    }

    // single-click on caret OR double-click on row toggles open/close
    auto toggle_open = [&]() {
      if (node.open) {
        node.open = false;
      } else {
        std::vector<SchemaNode> schemas;
        auto [ok, err] = connect_and_load(node.conn, schemas);
        if (ok) {
          node.open    = true;
          node.schemas = std::move(schemas);
        } else {
          app.msg_dlg = {true, "Connection error", std::move(err)};
        }
      }
    };
    if (click && h_caret)              toggle_open();
    if (dblclick && h_row && connected) toggle_open();
    if (rclick && (h_row || h_edit)) {
      float mx2 = std::min(app.mx, pw - PanelMenu::W - 2.f);
      float my2 = std::min(app.my, ph - PanelMenu::N * PanelMenu::IH - 10.f);
      app.panel_menu = PanelMenu{true, mx2, my2, i, connected};
    }
    row++;

    if (node.open) {
      for (auto &schema : node.schemas) {
        float sy = HDR_H + row * ITEM_H;
        if (hit(app.mx, app.my, 0, sy, pw, ITEM_H)) fill(ren, C_HOVER, 0, sy, pw, ITEM_H);
        fill(ren, C_ACCENT, PAD + 18, sy + (ITEM_H - 4) * .5f, 4, 4);
        text_draw(ren, schema.repo_name.c_str(), PAD + 26, center_baseline(sy, ITEM_H), C_TEXT);
        sc(ren, C_BORDER);
        SDL_FRect sl{0, sy + ITEM_H - 1, pw, 1};
        SDL_RenderFillRect(ren, &sl);
        row++;
      }
    }
  }

  sc(ren, C_BORDER);
  SDL_FRect div{pw - 1, 0, 1, ph};
  SDL_RenderFillRect(ren, &div);

  int  act      = app.panel_menu.render(ren, app.mx, app.my, click, rclick);
  int  ci       = app.panel_menu.conn_idx;
  bool valid_ci = ci >= 0 && ci < (int)app.conns.size();

  if (act == 0 && valid_ci) {
    ConnNode &node = app.conns[ci];
    if (node.conn.connected) {
      // Отсоединиться
      node.conn.connected = false;
      node.open           = false;
      node.schemas.clear();
      save_conn(node.conn);
    } else {
      // Присоединиться: verify connection, mark connected, persist
      auto [ok, err] = test_connection(node.conn.host, node.conn.port,
                                       node.conn.dbname, node.conn.user, node.conn.pass);
      if (ok) {
        node.conn.connected = true;
        save_conn(node.conn);
      } else {
        app.msg_dlg = {true, "Connection failed", std::move(err)};
      }
    }
  } else if (act == 1 && valid_ci) {
    app.repo_dlg.open_for(app.conns[ci].conn);
    app.repo_dlg.open = true;
    SDL_StartTextInput(app.win);
  } else if (act == 2) {
    app.dlg.open_add();
    app.dlg.open = true;
    SDL_StartTextInput(app.win);
  } else if (act == 3 && valid_ci) {
    app.dlg.open_edit(app.conns[ci].conn);
    app.dlg.open = true;
    SDL_StartTextInput(app.win);
  } else if (act == 4 && valid_ci) {
    app.pending_delete_idx = ci;
    app.confirm_dlg = {true, "Удаление соединения",
                       "Удалить соединение \"" + app.conns[ci].conn.name + "\"?"};
  }
}
