#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include "front/App.h"
#include "front/AppIcon.h"
#include "front/DlgMouse.h"
#include "front/FontAtlas.h"
#include "front/common.h"
#include "front/render_helpers.h"

#include "back/model/SchemaNode.h"
#include "back/pool/PoolService.h"
#include "back/service/ConnServiceRW.h"
#include "back/service/FolderServiceRW.h"
#include "back/service/RepoServiceR.h"
#include "back/service/UnitServiceRW.h"

int main(int /*argc*/, char * /*argv*/[])
{
  if (!SDL_Init(SDL_INIT_VIDEO)) {
    SDL_Log("SDL_Init: %s", SDL_GetError());
    return 1;
  }

  front::App app{};
  app.win = SDL_CreateWindow("probe_lang", app.ww, app.wh, SDL_WINDOW_RESIZABLE);
  if (!app.win) {
    SDL_Log("CreateWindow: %s", SDL_GetError());
    return 1;
  }
  app.ren = SDL_CreateRenderer(app.win, nullptr);
  if (!app.ren) {
    SDL_Log("CreateRenderer: %s", SDL_GetError());
    return 1;
  }

  // Window / taskbar icon: a procedurally drawn hedgehog (ёжик).
  if (SDL_Surface *icon = front::make_hedgehog_icon()) {
    SDL_SetWindowIcon(app.win, icon);
    SDL_DestroySurface(icon);
  }

  SDL_SetRenderDrawBlendMode(app.ren, SDL_BLENDMODE_NONE);
  front::font_init(app.ren);
  app.reload_conns();
  restore_tree_open_state(app);

  bool running = true;
  while (running) {
    bool  l_click = false, r_click = false;
    float l_click_x = 0, l_click_y = 0, r_click_x = 0, r_click_y = 0;
    int   l_clicks = 1;

    auto any_dlg = [&] {
      return app.dlg.open || app.repo_dlg.open || app.folder_dlg.open || app.unit_dlg.open || app.msg_dlg.open || app.confirm_dlg.open ||
             app.sel_unit_form.open;
    };

    // Shared form focus navigation: Tab/Shift+Tab cycles components; Enter on a
    // field jumps to the next component, on a button (index >= first_button) it
    // sets `activate` so the form's render presses it. Returns true if handled.
    auto form_nav = [](int &focus, bool &activate, int count, int first_button, SDL_Keycode key, SDL_Keymod mod) {
      const bool shift = (mod & SDL_KMOD_SHIFT) != 0;
      if (key == SDLK_TAB) {
        focus = (focus + (shift ? count - 1 : 1)) % count;
        return true;
      }
      if (key == SDLK_RETURN || key == SDLK_KP_ENTER) {
        if (focus >= first_button)
          activate = true;
        else
          focus = (focus + 1) % count;
        return true;
      }
      return false;
    };

    SDL_Event ev;
    while (SDL_PollEvent(&ev)) {
      switch (ev.type) {
        case SDL_EVENT_QUIT: running = false; break;

        case SDL_EVENT_WINDOW_RESIZED: SDL_GetWindowSize(app.win, &app.ww, &app.wh); break;

        case SDL_EVENT_MOUSE_MOTION:
          app.mx = ev.motion.x;
          app.my = ev.motion.y;
          if (app.dlg.open && app.lmb_held)
            for (auto &f : app.dlg.fields)
              f.on_move(ev.motion.x);
          if (app.repo_dlg.open) {
            if (app.lmb_held)
              for (auto &f : app.repo_dlg.fields)
                f.on_move(ev.motion.x);
            app.repo_dlg.err_view.on_move(ev.motion.x, ev.motion.y);
          }
          if (app.folder_dlg.open && app.lmb_held) app.folder_dlg.name_field.on_move(ev.motion.x);
          if (app.unit_dlg.open && app.lmb_held) app.unit_dlg.name_field.on_move(ev.motion.x);
          if (app.sel_unit_form.open && app.lmb_held) app.sel_unit_form.filter_field.on_move(ev.motion.x);
          if (app.editor.open) app.editor.on_mouse_move(ev.motion.x, ev.motion.y);
          break;

        case SDL_EVENT_MOUSE_BUTTON_DOWN:
          if (ev.button.button == SDL_BUTTON_LEFT) {
            app.lmb_held = true;
            l_click      = true;
            l_click_x    = ev.button.x;
            l_click_y    = ev.button.y;
            l_clicks     = ev.button.clicks;
          }
          if (ev.button.button == SDL_BUTTON_RIGHT) {
            r_click   = true;
            r_click_x = ev.button.x;
            r_click_y = ev.button.y;
          }
          if (ev.button.button == SDL_BUTTON_MIDDLE && app.editor.open && !any_dlg()) app.editor.on_middle_down(ev.button.x, ev.button.y);
          break;

        case SDL_EVENT_MOUSE_BUTTON_UP:
          if (ev.button.button == SDL_BUTTON_LEFT) {
            app.lmb_held = false;
            if (app.dlg.open)
              for (auto &f : app.dlg.fields)
                f.on_release();
            if (app.repo_dlg.open) {
              for (auto &f : app.repo_dlg.fields)
                f.on_release();
              app.repo_dlg.err_view.on_release();
            }
            if (app.folder_dlg.open) app.folder_dlg.name_field.on_release();
            if (app.unit_dlg.open) app.unit_dlg.name_field.on_release();
            if (app.sel_unit_form.open) app.sel_unit_form.filter_field.on_release();
            if (app.editor.open) app.editor.on_mouse_up();
          }
          if (ev.button.button == SDL_BUTTON_MIDDLE && app.editor.open) app.editor.on_middle_up();
          break;

        case SDL_EVENT_MOUSE_WHEEL:
          if (app.sel_unit_form.open && app.sel_unit_form.mouse_over_list(app.mx, app.my))
            app.sel_unit_form.on_scroll(ev.wheel.y);
          else if (app.repo_dlg.open && app.repo_dlg.err_view.mouse_over(app.mx, app.my))
            app.repo_dlg.err_view.on_scroll(ev.wheel.y);
          else if (app.editor.open && !any_dlg())
            app.editor.on_wheel(ev.wheel.y, app.mx, app.my);
          break;

        case SDL_EVENT_TEXT_INPUT:
          if (app.dlg.open) {
            app.dlg.fields[app.dlg.focus].handle_text(ev.text.text);
          } else if (app.repo_dlg.open) {
            if (app.repo_dlg.focus < app.repo_dlg.FIRST_BUTTON) app.repo_dlg.fields[app.repo_dlg.focus].handle_text(ev.text.text);
          } else if (app.folder_dlg.open) {
            if (app.folder_dlg.focus == 0) app.folder_dlg.name_field.handle_text(ev.text.text);
          } else if (app.unit_dlg.open) {
            if (app.unit_dlg.focus == 0) app.unit_dlg.name_field.handle_text(ev.text.text);
          } else if (app.sel_unit_form.open) {
            if (app.sel_unit_form.focus == 0) app.sel_unit_form.filter_field.handle_text(ev.text.text);
          } else if (app.editor.editing) {
            app.editor.handle_text(ev.text.text);
          }
          break;

        case SDL_EVENT_KEY_DOWN:
          if (app.dlg.open && (ev.key.mod & SDL_KMOD_CTRL) && (ev.key.key == SDLK_RETURN || ev.key.key == SDLK_KP_ENTER)) {
            // Ctrl+Enter presses Save from anywhere — only when Save is active.
            if (app.dlg.save_enabled()) {
              if (back::model::ConnStore c = app.dlg.to_conn(); !c.name.empty()) {
                back::save_conn(c, app.dlg.old_name);
                app.reload_conns();
                app.dlg.open = false;
                SDL_StopTextInput(app.win);
              } else {
                app.dlg.err = "Name is required";
              }
            }
          } else if (app.dlg.open) {
            // ReSharper disable once CppUseStructuredBinding
            for (front::InputField &f : app.dlg.fields)
              f.ctx.open = false;
            SDL_Keymod mod = ev.key.mod;
            // ReSharper disable once CppTooWideScopeInitStatement
            bool consumed  = app.dlg.fields[app.dlg.focus].handle_key(ev.key.key, mod);
            if (!consumed) {
              bool shift = (mod & SDL_KMOD_SHIFT) != 0;
              switch (ev.key.key) {
                case SDLK_TAB: app.dlg.focus = (app.dlg.focus + (shift ? 5 : 1)) % 6; break;
                case SDLK_ESCAPE:
                  app.dlg.open = false;
                  SDL_StopTextInput(app.win);
                  break;
                case SDLK_RETURN:
                case SDLK_KP_ENTER:
                  if (app.dlg.focus < 5) {
                    app.dlg.focus++;
                  } else if (back::model::ConnStore c = app.dlg.to_conn(); !c.name.empty() && !c.host.empty()) {
                    back::save_conn(c, app.dlg.old_name);
                    app.reload_conns();
                    app.dlg.open = false;
                    SDL_StopTextInput(app.win);
                  } else {
                    app.dlg.err = c.name.empty() ? "Name is required" : "Host is required";
                  }
                  break;
                default: break;
              }
            }
          } else if (app.repo_dlg.open) {
            auto      &d   = app.repo_dlg;
            SDL_Keymod mod = ev.key.mod;
            const bool ent = ev.key.key == SDLK_RETURN || ev.key.key == SDLK_KP_ENTER;
            if ((mod & SDL_KMOD_CTRL) && ent) { // Ctrl+Enter presses OK (render gates on active)
              d.focus    = d.SAVE;
              d.activate = true;
            } else {
              bool consumed = false;
              if (d.err_view.focused) consumed = d.err_view.handle_key(ev.key.key, mod);
              if (!consumed && d.focus < d.FIRST_BUTTON) consumed = d.fields[d.focus].handle_key(ev.key.key, mod);
              if (!consumed && !form_nav(d.focus, d.activate, d.FOCUS_COUNT, d.FIRST_BUTTON, ev.key.key, mod) && ev.key.key == SDLK_ESCAPE) {
                d.open = false;
                SDL_StopTextInput(app.win);
              }
            }
          } else if (app.folder_dlg.open) {
            auto      &d   = app.folder_dlg;
            SDL_Keymod mod = ev.key.mod;
            const bool ent = ev.key.key == SDLK_RETURN || ev.key.key == SDLK_KP_ENTER;
            if ((mod & SDL_KMOD_CTRL) && ent) {
              d.focus    = d.SAVE;
              d.activate = true;
            } else {
              bool consumed = d.focus == 0 && d.name_field.handle_key(ev.key.key, mod);
              if (!consumed && !form_nav(d.focus, d.activate, d.FOCUS_COUNT, d.FIRST_BUTTON, ev.key.key, mod) && ev.key.key == SDLK_ESCAPE) {
                d.open = false;
                SDL_StopTextInput(app.win);
              }
            }
          } else if (app.unit_dlg.open) {
            auto      &d   = app.unit_dlg;
            SDL_Keymod mod = ev.key.mod;
            const bool ent = ev.key.key == SDLK_RETURN || ev.key.key == SDLK_KP_ENTER;
            if ((mod & SDL_KMOD_CTRL) && ent) {
              d.focus    = d.SAVE;
              d.activate = true;
            } else {
              bool consumed = d.focus == 0 && d.name_field.handle_key(ev.key.key, mod);
              if (!consumed && !form_nav(d.focus, d.activate, d.FOCUS_COUNT, d.FIRST_BUTTON, ev.key.key, mod) && ev.key.key == SDLK_ESCAPE) {
                d.open = false;
                SDL_StopTextInput(app.win);
              }
            }
          } else if (app.sel_unit_form.open) {
            front::FormSelectUnit &d        = app.sel_unit_form;
            SDL_Keymod             mod      = ev.key.mod;
            bool                   consumed = d.focus == 0 && d.filter_field.handle_key(ev.key.key, mod);
            if (!consumed && !form_nav(d.focus, d.activate, d.FOCUS_COUNT, d.FIRST_BUTTON, ev.key.key, mod) && ev.key.key == SDLK_ESCAPE) {
              d.close();
              SDL_StopTextInput(app.win);
            }
          } else if (app.editor.editing) {
            SDL_Keymod mod = ev.key.mod;
            app.editor.handle_key(ev.key.key, mod);
          } else if (app.editor.open && ev.key.key == SDLK_ESCAPE) {
            app.editor.close();
          } else {
            // No dialog open: tree/menu keyboard navigation.
            panel_key_down(app, ev.key.key);
          }
          break;

        default: break;
      }
    }

    // ── render ────────────────────────────────────────────────────────────────
    bool dblclick = l_click && l_clicks >= 2;

    sc(app.ren, front::C_BG);
    SDL_RenderClear(app.ren);
    panel_render(app.ren, app, l_click, r_click, dblclick);

    float pw = app.ww * 0.30f;
    fill(app.ren, front::C_BG, pw, 0, app.ww - pw, static_cast<float>(app.wh));

    if (app.editor.open) {
      bool        editor_clicks = !any_dlg();
      static bool prev_editing  = false;
      app.editor.render(app.ren,
                        pw,
                        0,
                        app.ww - pw,
                        static_cast<float>(app.wh),
                        app.mx,
                        app.my,
                        editor_clicks && l_click,
                        editor_clicks && r_click,
                        l_clicks);

      if (app.editor.editing && !prev_editing) SDL_StartTextInput(app.win);
      if (!app.editor.editing && prev_editing) SDL_StopTextInput(app.win);
      prev_editing = app.editor.editing;

      // ContextMenuSelExpr picked "Юнит": open the unit-selection form.
      if (app.editor.want_sel_unit) {
        app.editor.want_sel_unit = false;
        if (auto *t = app.editor.cur()) {
          app.sel_unit_form.open_for(t->conn, t->schema, app.editor.want_sel_unit_field_id);
          SDL_StartTextInput(app.win);
        }
      }
    }

    if (app.dlg.open) {
      front::DlgMouse dm;
      dm.mx     = l_click ? l_click_x : (r_click ? r_click_x : app.mx);
      dm.my     = l_click ? l_click_y : (r_click ? r_click_y : app.my);
      dm.ldown  = l_click;
      dm.rdown  = r_click;
      dm.clicks = l_clicks;

      if (int result = dlg_render(app.ren, app.dlg, dm); result == 1) {
        back::save_conn(app.dlg.to_conn(), app.dlg.old_name);
        app.reload_conns();
        app.dlg.open = false;
        SDL_StopTextInput(app.win);
      } else if (result == -1) {
        app.dlg.open = false;
        SDL_StopTextInput(app.win);
      }
    }

    if (app.repo_dlg.open) {
      float mx2 = l_click ? l_click_x : app.mx;
      float my2 = l_click ? l_click_y : app.my;
      // ReSharper disable once CppTooWideScopeInitStatement
      int res   = app.repo_dlg.render(app.ren, mx2, my2, l_click, r_click, l_clicks);
      if (res == 1) {
        // ReSharper disable once CppUseStructuredBinding
        for (back::model::ConnNode &node : app.conns) {
          if (node.conn.name == app.repo_dlg.conn.name) {
            if (!app.repo_dlg.editing) {
              node.conn.connected = true;
              back::save_conn(node.conn);
            }
            if (node.open) {
              std::vector<back::model::RepoNode> repos;
              // ReSharper disable once CppTooWideScopeInitStatement
              auto [ok, err] = back::connect_and_load(node.conn.conn(), repos);
              if (ok) {
                node.repos = std::move(repos);
                front::restore_open_repos_and_folders(node); // keep already-open repos/folders open
              }
            }
            break;
          }
        }
        app.repo_dlg.open = false;
        SDL_StopTextInput(app.win);
      } else if (res == -1) {
        app.repo_dlg.open = false;
        SDL_StopTextInput(app.win);
      }
    }

    if (app.folder_dlg.open) {
      float mx2 = l_click ? l_click_x : app.mx;
      float my2 = l_click ? l_click_y : app.my;
      // ReSharper disable once CppTooWideScopeInitStatement
      int res   = app.folder_dlg.render(app.ren, mx2, my2, l_click, r_click, l_clicks);
      if (res == 1) {
        int fci = app.folder_dlg.conn_idx;
        // ReSharper disable once CppTooWideScopeInitStatement
        int fri = app.folder_dlg.repo_idx;
        if (fci >= 0 && fci < static_cast<int>(app.conns.size()) && fri >= 0 && fri < static_cast<int>(app.conns[fci].repos.size())) {
          auto             &node      = app.conns[fci];
          auto             &repo      = node.repos[fri];
          const bool        editing   = app.folder_dlg.editing;
          const std::string parent_id = app.folder_dlg.parent_folder_id;

          auto [ok, err] = back::load_repo_tree(node.conn.conn(), repo.schema_name, repo);
          if (!ok) {
            app.msg_dlg = {true, "Ошибка", std::move(err)};
          } else {
            front::restore_repo_folders_open(node.conn.name, repo); // keep already-open folders open
            if (!editing) front::open_added_folder_parent(node.conn.name, repo, parent_id);
          }
        }
        app.folder_dlg.open = false;
        SDL_StopTextInput(app.win);
      } else if (res == -1) {
        app.folder_dlg.open = false;
        SDL_StopTextInput(app.win);
      }
    }

    if (app.unit_dlg.open) {
      float mx2 = l_click ? l_click_x : app.mx;
      float my2 = l_click ? l_click_y : app.my;
      // ReSharper disable once CppTooWideScopeInitStatement
      int res   = app.unit_dlg.render(app.ren, mx2, my2, l_click, r_click, l_clicks);
      if (res == 1) {
        int uci = app.unit_dlg.conn_idx;
        // ReSharper disable once CppTooWideScopeInitStatement
        int uri = app.unit_dlg.repo_idx;
        if (uci >= 0 && uci < static_cast<int>(app.conns.size()) && uri >= 0 && uri < static_cast<int>(app.conns[uci].repos.size())) {
          back::model::ConnNode &node      = app.conns[uci];
          back::model::RepoNode &repo      = node.repos[uri];
          const bool             editing   = app.unit_dlg.editing;
          const std::string      parent_id = app.unit_dlg.parent_folder_id;

          auto [ok, err] = back::load_repo_tree(node.conn.conn(), repo.schema_name, repo);
          if (!ok) {
            app.msg_dlg = {true, "Ошибка", std::move(err)};
          } else {
            front::restore_repo_folders_open(node.conn.name, repo);                         // keep already-open folders open
            if (!editing) front::open_added_folder_parent(node.conn.name, repo, parent_id); // reveal the new unit
          }
        }
        app.unit_dlg.open = false;
        SDL_StopTextInput(app.win);
      } else if (res == -1) {
        app.unit_dlg.open = false;
        SDL_StopTextInput(app.win);
      }
    }

    if (app.sel_unit_form.open) {
      float             mx2 = l_click ? l_click_x : app.mx;
      float             my2 = l_click ? l_click_y : app.my;
      const std::string fid = app.sel_unit_form.field_id;
      // ReSharper disable once CppTooWideScopeInitStatement
      int res               = app.sel_unit_form.render(app.ren, mx2, my2, l_click, r_click, l_clicks);
      if (res != 0) SDL_StopTextInput(app.win);                     // form closes itself on confirm/cancel
      if (res == 1 && app.editor.open) app.editor.refit_field(fid); // show the chosen unit; resize the box
    }

    if (app.msg_dlg.open) {
      if (app.msg_dlg.render(app.ren, app.mx, app.my, l_click)) app.msg_dlg.open = false;
    }

    if (app.confirm_dlg.open) {
      // ReSharper disable once CppTooWideScopeInitStatement
      int res = app.confirm_dlg.render(app.ren, app.mx, app.my, l_click);
      if (res == 1) {
        if (app.pending_delete_idx >= 0) {
          // ReSharper disable once CppTooWideScopeInitStatement
          int idx = app.pending_delete_idx;
          if (idx < static_cast<int>(app.conns.size())) {
            back::pool::closeConnectionPool(app.conns[idx].conn.conn()); // drop the pooled connections for the removed connection
            back::delete_conn(app.conns[idx].conn.name);
            app.reload_conns();
          }
          app.pending_delete_idx = -1;
        } else if (!app.pending_delete_folder_id.empty()) {
          int fci = app.pending_delete_folder_conn;
          // ReSharper disable once CppTooWideScopeInitStatement
          int fri = app.pending_delete_folder_repo;
          if (fci >= 0 && fci < static_cast<int>(app.conns.size()) && fri >= 0 && fri < static_cast<int>(app.conns[fci].repos.size())) {
            auto &repo     = app.conns[fci].repos[fri];
            // ReSharper disable once CppTooWideScopeInitStatement
            auto [ok, err] = back::delete_folder_recursive(app.conns[fci].conn.conn(), repo.schema_name, app.pending_delete_folder_id);
            if (ok) {
              // ReSharper disable once CppTooWideScopeInitStatement
              auto [ok2, err2] = back::load_repo_tree(app.conns[fci].conn.conn(), repo.schema_name, repo);
              if (!ok2)
                app.msg_dlg = {true, "Ошибка", std::move(err2)};
              else
                front::restore_repo_folders_open(app.conns[fci].conn.name, repo); // keep already-open folders open
            } else {
              app.msg_dlg = {true, "Ошибка", std::move(err)};
            }
          }
          app.pending_delete_folder_conn = -1;
          app.pending_delete_folder_repo = -1;
          app.pending_delete_folder_id.clear();
        } else if (!app.pending_delete_unit_id.empty()) {
          int uci = app.pending_delete_unit_conn;
          // ReSharper disable once CppTooWideScopeInitStatement
          int uri = app.pending_delete_unit_repo;
          if (uci >= 0 && uci < static_cast<int>(app.conns.size()) && uri >= 0 && uri < static_cast<int>(app.conns[uci].repos.size())) {
            auto &repo     = app.conns[uci].repos[uri];
            // ReSharper disable once CppTooWideScopeInitStatement
            auto [ok, err] = back::delete_unit(app.conns[uci].conn.conn(), repo.schema_name, app.pending_delete_unit_id);
            if (ok) {
              // ReSharper disable once CppTooWideScopeInitStatement
              auto [ok2, err2] = back::load_repo_tree(app.conns[uci].conn.conn(), repo.schema_name, repo);
              if (!ok2)
                app.msg_dlg = {true, "Ошибка", std::move(err2)};
              else
                front::restore_repo_folders_open(app.conns[uci].conn.name, repo); // keep already-open folders open
            } else {
              app.msg_dlg = {true, "Ошибка", std::move(err)};
            }
          }
          app.pending_delete_unit_conn = -1;
          app.pending_delete_unit_repo = -1;
          app.pending_delete_unit_id.clear();
        }
      } else if (res == -1) {
        app.pending_delete_idx         = -1;
        app.pending_delete_folder_conn = -1;
        app.pending_delete_folder_repo = -1;
        app.pending_delete_folder_id.clear();
        app.pending_delete_unit_conn = -1;
        app.pending_delete_unit_repo = -1;
        app.pending_delete_unit_id.clear();
      }
    }

    SDL_RenderPresent(app.ren);
    SDL_Delay(16);
  }

  back::pool::closeAllConnectionPools(); // close every pooled DB connection before shutting down

  SDL_DestroyRenderer(app.ren);
  SDL_DestroyWindow(app.win);
  SDL_Quit();
  return 0;
}
