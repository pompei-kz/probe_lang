#include "common.h"
#include "ContextMenu.h"
#include "FontAtlas.h"
#include "PanelMenu.h"
#include "SchemaMenu.h"
#include "back/ConnService.h"
#include "back/ProjectTreeService.h"
#include "back/RepoService.h"
#include "back/model/ConnNode.h"
#include "render_helpers.h"

namespace front {

  using namespace back;
  using namespace back::model;

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

    float clamp_cx = dx + DW - ContextMenu::W - 4.f;
    float clamp_cy = dy + DH - ContextMenu::N * ContextMenu::IH - 10.f;

    for (auto &f : fields) {
      float fy      = ct + f.idx * FS_STEP;
      float by      = fy + FS + 6;
      bool  focused = (d.focus == f.idx);

      text_draw(ren, f.lbl, dx + 16, fy + FS, C_DIM);
      d.fields[f.idx].draw(ren, dx + 16, by, fw, FH, focused);

      if (m.ldown) {
        if (d.fields[f.idx].on_ldown(text_ox, m.mx, m.my, dx + 16, by, fw, FH, m.clicks)) d.focus = f.idx;
      }
      if (m.rdown) {
        if (d.fields[f.idx].on_rdown(m.mx, m.my, dx + 16, by, fw, FH, clamp_cx, clamp_cy)) d.focus = f.idx;
      }
    }

    bool any_ctx = false;
    for (int i = 0; i < 6; i++)
      if (d.fields[i].ctx.open) {
        any_ctx = true;
        break;
      }

    // ── Test Connection button ────────────────────────────────────────────────
    constexpr float BH = 30.f, BW_T = 170.f, BW_S = 90.f, BW_C = 80.f;
    float           test_btn_y = ct + 6 * FS_STEP + 2;
    bool            h_test     = hit(m.mx, m.my, dx + 16, test_btn_y, BW_T, BH);
    fill(ren, h_test ? C_HOVER : C_BORDER, dx + 16, test_btn_y, BW_T, BH);
    text_draw(ren, "Test connection", dx + 16 + (BW_T - text_w("Test connection")) * .5f, center_baseline(test_btn_y, BH), C_TEXT);

    if (m.ldown && h_test && !any_ctx) {
      d.err          = "";
      auto [ok, msg] = test_connection(d.fields[1].ed.buf, d.fields[2].ed.buf, d.fields[3].ed.buf, d.fields[4].ed.buf, d.fields[5].ed.buf);
      d.test_ok      = ok;
      d.test_msg     = msg;
      if (ok) {
        d.snap_host   = d.fields[1].ed.buf;
        d.snap_port   = d.fields[2].ed.buf;
        d.snap_dbname = d.fields[3].ed.buf;
        d.snap_user   = d.fields[4].ed.buf;
        d.snap_pass   = d.fields[5].ed.buf;
      }
    }

    // test result message
    if (!d.test_msg.empty()) text_draw(ren, d.test_msg.c_str(), dx + 16, test_btn_y + 38, d.test_ok ? C_OK : C_ERR);

    // validation error
    if (!d.err.empty()) text_draw(ren, d.err.c_str(), dx + 16, test_btn_y + 60, C_ERR);

    // ── Save / Cancel buttons ─────────────────────────────────────────────────
    float btn_y    = dy + DH - 52;
    bool  can_save = d.save_enabled();
    float sx       = dx + DW - 16 - BW_S;
    float cx2      = sx - 10 - BW_C;

    bool h_save = can_save && hit(m.mx, m.my, sx, btn_y, BW_S, BH);
    bool h_can  = hit(m.mx, m.my, cx2, btn_y, BW_C, BH);
    fill(ren, can_save ? (h_save ? C_ACCENT : C_BORDER) : C_DIM, sx, btn_y, BW_S, BH);
    fill(ren, h_can ? C_HOVER : C_BORDER, cx2, btn_y, BW_C, BH);

    auto btn_text = [&](const char *t, float bx, float bw, Clr c) { text_draw(ren, t, bx + (bw - text_w(t)) * .5f, center_baseline(btn_y, BH), c); };
    btn_text("Save", sx, BW_S, can_save ? (h_save ? C_PANEL : C_TEXT) : C_DIM);
    btn_text("Cancel", cx2, BW_C, C_TEXT);

    // context menus rendered on top of everything
    for (int i = 0; i < 6; i++)
      d.fields[i].render_ctx(ren, m.mx, m.my, m.ldown, m.rdown);

    if (m.ldown && !any_ctx) {
      if (h_can) return -1;
      if (h_save && can_save) {
        Conn c = d.to_conn();
        if (c.name.empty()) {
          d.err = "Name is required";
          return 0;
        }
        return 1;
      }
    }
    return 0;
  }

  // ── left panel helpers ────────────────────────────────────────────────────────
  static constexpr float ITEM_H = 30.f, HDR_H = 38.f, PAD = 10.f;
  static constexpr float ICON_SZ = 8.f, EDIT_W = 30.f, CARET_W = 22.f;

  // Defined further down; reopens persisted descendants of a folder from disk.
  static void restore_folder_open(std::vector<std::string> prefix, FolderNode &folder);

  // Right-pointing (collapsed) or down-pointing (expanded) triangle
  static void draw_caret(SDL_Renderer *r, float cx, float cy, bool open, Clr c)
  {
    sc(r, c);
    if (open) {
      // ▼
      SDL_RenderLine(r, cx - 5, cy - 2, cx + 5, cy - 2);
      SDL_RenderLine(r, cx - 5, cy - 2, cx, cy + 3);
      SDL_RenderLine(r, cx + 5, cy - 2, cx, cy + 3);
      SDL_RenderLine(r, cx - 3, cy, cx + 3, cy);
    } else {
      // ▶
      SDL_RenderLine(r, cx - 2, cy - 5, cx - 2, cy + 5);
      SDL_RenderLine(r, cx - 2, cy - 5, cx + 3, cy);
      SDL_RenderLine(r, cx - 2, cy + 5, cx + 3, cy);
      SDL_RenderLine(r, cx - 2, cy, cx, cy);
    }
  }

  static void draw_folder_icon(SDL_Renderer *r, float x, float y, Clr c)
  {
    sc(r, c);
    SDL_FRect tab{x, y - 2.f, 6.f, 3.f};
    SDL_RenderFillRect(r, &tab);
    SDL_FRect body{x, y, 10.f, 7.f};
    SDL_RenderFillRect(r, &body);
  }

  static void render_folder_list(SDL_Renderer                   *ren,
                                 App                            &app,
                                 std::vector<FolderNode>        &folders,
                                 int                             ci,
                                 int                             ri,
                                 const std::vector<std::string> &prefix,
                                 int                             depth,
                                 int                            &row,
                                 float                           pw,
                                 float                           ph,
                                 bool                            click,
                                 bool                            rclick,
                                 bool                            dblclick)
  {
    static constexpr float STEP = 14.f;
    for (auto &folder : folders) {
      float      iy       = HDR_H + row * ITEM_H;
      float      ix       = PAD + CARET_W + (depth + 1) * STEP;
      const bool has_kids = !folder.children.empty();
      bool       h_caret  = has_kids && hit(app.mx, app.my, 0, iy, ix, ITEM_H);
      bool       h_row    = !h_caret && hit(app.mx, app.my, 0, iy, pw, ITEM_H);
      if (h_caret || h_row) fill(ren, C_HOVER, 0, iy, pw, ITEM_H);

      if (has_kids) draw_caret(ren, ix - 9.f, iy + ITEM_H * .5f, folder.open, folder.open ? C_ACCENT : C_DIM);

      draw_folder_icon(ren, ix, iy + ITEM_H * .5f - 3.f, C_DIM);
      text_draw(ren, folder.name.c_str(), ix + 14.f, center_baseline(iy, ITEM_H), C_TEXT);
      sc(ren, C_BORDER);
      SDL_FRect sl{0, iy + ITEM_H - 1, pw, 1};
      SDL_RenderFillRect(ren, &sl);

      std::vector<std::string> path = prefix;
      path.push_back(folder.id);

      // caret single-click OR row double-click toggles a branch
      if (has_kids && ((click && h_caret) || (dblclick && h_row))) {
        folder.open = !folder.open;
        if (folder.open) {
          open_tree_node(path);
          // Always consult disk for which children were left open.
          for (auto &child : folder.children)
            restore_folder_open(path, child);
        } else {
          close_tree_node(path);
        }
      }

      if (rclick && (h_caret || h_row)) {
        float mx2           = std::min(app.mx, pw - FolderMenu::W - 2.f);
        float my2           = std::min(app.my, ph - FolderMenu::N * FolderMenu::IH - 10.f);
        app.folder_menu     = {true, mx2, my2, ci, ri, folder.id, folder.name};
        app.panel_menu.open = false;
        app.repo_menu.open  = false;
      }
      row++;
      if (folder.open) {
        render_folder_list(ren, app, folder.children, ci, ri, path, depth + 1, row, pw, ph, click, rclick, dblclick);
      }
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
      ConnNode  &node      = app.conns[i];
      const bool connected = node.conn.connected;
      float      iy        = HDR_H + row * ITEM_H;

      bool h_caret = connected && hit(app.mx, app.my, 0, iy, PAD + CARET_W, ITEM_H);
      bool h_row   = !h_caret && hit(app.mx, app.my, 0, iy, pw - EDIT_W, ITEM_H);
      bool h_edit  = hit(app.mx, app.my, pw - EDIT_W, iy, EDIT_W, ITEM_H);
      if (h_row || h_caret) app.h_item = i;
      if (h_edit) app.h_edit = i;
      if (h_row || h_caret || h_edit) fill(ren, C_HOVER, 0, iy, pw, ITEM_H);

      // caret on the left (only for connected nodes)
      float caret_cx = PAD + CARET_W * .5f;
      float caret_cy = iy + ITEM_H * .5f;
      if (connected) draw_caret(ren, caret_cx, caret_cy, node.open, node.open ? C_ACCENT : C_DIM);

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
          close_tree_node({node.conn.name});
        } else {
          std::vector<RepoNode> repos;
          auto [ok, err] = connect_and_load(node.conn, repos);
          if (ok) {
            node.open  = true;
            node.repos = std::move(repos);
            open_tree_node({node.conn.name});
            restore_open_repos_and_folders(node); // reopen persisted repos/folders from disk
          } else {
            app.msg_dlg = {true, "Connection error", std::move(err)};
          }
        }
      };
      if (click && h_caret) toggle_open();
      if (dblclick && h_row && connected) toggle_open();
      if (rclick && (h_row || h_edit)) {
        float mx2            = std::min(app.mx, pw - PanelMenu::W - 2.f);
        float my2            = std::min(app.my, ph - PanelMenu::N * PanelMenu::IH - 10.f);
        app.panel_menu       = PanelMenu{true, mx2, my2, i, connected};
        app.repo_menu.open   = false;
        app.folder_menu.open = false;
      }
      row++;

      if (node.open) {
        for (int ri = 0; ri < (int)node.repos.size(); ri++) {
          auto &repo     = node.repos[ri];
          float sy       = HDR_H + row * ITEM_H;
          float caret_x  = PAD + CARET_W; // caret zone right edge
          bool  has_kids = !repo.folders.empty();
          bool  h_caret  = has_kids && hit(app.mx, app.my, 0, sy, caret_x, ITEM_H);
          bool  h_row    = !h_caret && hit(app.mx, app.my, 0, sy, pw, ITEM_H);
          if (h_caret || h_row) fill(ren, C_HOVER, 0, sy, pw, ITEM_H);

          if (has_kids) draw_caret(ren, PAD + CARET_W * .5f, sy + ITEM_H * .5f, repo.open, repo.open ? C_ACCENT : C_DIM);

          fill(ren, C_ACCENT, PAD + CARET_W + 4, sy + (ITEM_H - 4) * .5f, 4, 4);
          text_draw(ren, repo.repo_name.c_str(), PAD + CARET_W + 12, center_baseline(sy, ITEM_H), C_TEXT);
          sc(ren, C_BORDER);
          SDL_FRect sl{0, sy + ITEM_H - 1, pw, 1};
          SDL_RenderFillRect(ren, &sl);

          // caret single-click OR row double-click toggles a branch
          if (has_kids && ((click && h_caret) || (dblclick && h_row))) {
            repo.open = !repo.open;
            if (repo.open) {
              open_tree_node({node.conn.name, repo.schema_name});
              restore_repo_folders_open(node.conn.name, repo); // reopen persisted folders from disk
            } else {
              close_tree_node({node.conn.name, repo.schema_name});
            }
          }
          if (rclick && (h_caret || h_row)) {
            float mx2            = std::min(app.mx, pw - RepoMenu::W - 2.f);
            float my2            = std::min(app.my, ph - RepoMenu::N * RepoMenu::IH - 10.f);
            app.repo_menu        = RepoMenu{true, mx2, my2, i, ri};
            app.panel_menu.open  = false;
            app.folder_menu.open = false;
          }
          row++;
          if (repo.open)
            render_folder_list(ren, app, repo.folders, i, ri, {node.conn.name, repo.schema_name}, 0, row, pw, ph, click, rclick, dblclick);
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
        node.repos.clear();
        save_conn(node.conn);
        close_tree_node({node.conn.name});
      } else {
        // Присоединиться: verify connection, mark connected, persist
        auto [ok, err] = test_connection(node.conn.host, node.conn.port, node.conn.dbname, node.conn.user, node.conn.pass);
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
      app.confirm_dlg        = {true, "Удаление соединения", "Удалить соединение \"" + app.conns[ci].conn.name + "\"?"};
    }

    int  ract      = app.repo_menu.render(ren, app.mx, app.my, click, rclick);
    int  rci       = app.repo_menu.conn_idx;
    int  rri       = app.repo_menu.repo_idx;
    bool valid_rci = rci >= 0 && rci < (int)app.conns.size() && rri >= 0 && rri < (int)app.conns[rci].repos.size();

    if (ract == 0 && valid_rci) {
      app.repo_dlg.open_edit_for(app.conns[rci].conn, app.conns[rci].repos[rri]);
      app.repo_dlg.open = true;
      SDL_StartTextInput(app.win);
    } else if (ract == 1 && valid_rci) {
      app.folder_dlg.open_add(rci, rri, app.conns[rci].conn, app.conns[rci].repos[rri].schema_name, "");
      app.folder_dlg.open = true;
      SDL_StartTextInput(app.win);
    }

    int  fmact    = app.folder_menu.render(ren, app.mx, app.my, click, rclick);
    int  fmci     = app.folder_menu.conn_idx;
    int  fmri     = app.folder_menu.repo_idx;
    bool valid_fm = fmci >= 0 && fmci < (int)app.conns.size() && fmri >= 0 && fmri < (int)app.conns[fmci].repos.size();

    if (valid_fm) {
      if (fmact == 0) {
        app.folder_dlg.open_add(fmci, fmri, app.conns[fmci].conn, app.conns[fmci].repos[fmri].schema_name, app.folder_menu.folder_id);
        app.folder_dlg.open = true;
        SDL_StartTextInput(app.win);
      } else if (fmact == 1) {
        app.folder_dlg.open_edit(fmci,
                                 fmri,
                                 app.conns[fmci].conn,
                                 app.conns[fmci].repos[fmri].schema_name,
                                 app.folder_menu.folder_id,
                                 app.folder_menu.folder_name);
        app.folder_dlg.open = true;
        SDL_StartTextInput(app.win);
      } else if (fmact == 2) {
        app.pending_delete_folder_conn = fmci;
        app.pending_delete_folder_repo = fmri;
        app.pending_delete_folder_id   = app.folder_menu.folder_id;
        app.confirm_dlg                = {true, "Удаление папки", "Удалить папку \"" + app.folder_menu.folder_name + "\" и все вложенные подпапки?"};
      }
    }
  }

  // Recursively reopen folders whose marker file exists. A folder's children
  // only "appear" once the folder itself is open, so we descend only then.
  static void restore_folder_open(std::vector<std::string> prefix, FolderNode &folder)
  {
    prefix.push_back(folder.id);
    if (is_tree_node_open(prefix)) {
      folder.open = true;
      for (auto &child : folder.children)
        restore_folder_open(prefix, child);
    }
  }

  void restore_repo_folders_open(const std::string &conn_name, RepoNode &repo)
  {
    for (auto &folder : repo.folders)
      restore_folder_open({conn_name, repo.schema_name}, folder);
  }

  void restore_open_repos_and_folders(ConnNode &node)
  {
    for (auto &repo : node.repos) {
      if (is_tree_node_open({node.conn.name, repo.schema_name})) repo.open = true;
      restore_repo_folders_open(node.conn.name, repo);
    }
  }

  void restore_tree_open_state(App &app)
  {
    for (auto &node : app.conns) {
      // Roots: only connected connections whose marker file exists.
      if (!node.conn.connected) continue;
      if (!is_tree_node_open({node.conn.name})) continue;

      std::vector<RepoNode> repos;
      auto [ok, err] = connect_and_load(node.conn, repos);
      if (!ok) continue;
      node.open  = true;
      node.repos = std::move(repos);

      restore_open_repos_and_folders(node);
    }
  }

  // Find folder `target` in the tree, open it and persist its marker.
  static bool open_folder_branch_rec(std::vector<std::string> prefix, std::vector<FolderNode> &folders, const std::string &target)
  {
    for (auto &folder : folders) {
      std::vector<std::string> path = prefix;
      path.push_back(folder.id);
      if (folder.id == target) {
        folder.open = true;
        open_tree_node(path);
        return true;
      }
      if (open_folder_branch_rec(path, folder.children, target)) return true;
    }
    return false;
  }

  void open_added_folder_parent(const std::string &conn_name, RepoNode &repo, const std::string &parent_folder_id)
  {
    if (parent_folder_id.empty()) {
      // Parent is the repo itself: already open while adding, just persist.
      repo.open = true;
      open_tree_node({conn_name, repo.schema_name});
      return;
    }
    open_folder_branch_rec({conn_name, repo.schema_name}, repo.folders, parent_folder_id);
  }

} // namespace front
