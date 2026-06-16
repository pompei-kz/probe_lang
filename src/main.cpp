#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include "front/App.h"
#include "front/DlgMouse.h"
#include "front/FontAtlas.h"
#include "front/common.h"
#include "front/render_helpers.h"

#include "back/ConnService.h"
#include "back/FolderService.h"
#include "back/RepoService.h"
#include "back/UnitService.h"
#include "back/model/SchemaNode.h"

using namespace front;
using namespace back;
using namespace back::model;

int main(int /*argc*/, char * /*argv*/[])
{
  if (!SDL_Init(SDL_INIT_VIDEO)) {
    SDL_Log("SDL_Init: %s", SDL_GetError());
    return 1;
  }

  App app{};
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

  SDL_SetRenderDrawBlendMode(app.ren, SDL_BLENDMODE_NONE);
  font_init(app.ren);
  app.reload_conns();
  restore_tree_open_state(app);

  bool running = true;
  while (running) {
    bool  l_click = false, r_click = false;
    float l_click_x = 0, l_click_y = 0, r_click_x = 0, r_click_y = 0;
    int   l_clicks = 1;

    auto any_dlg = [&] {
      return app.dlg.open || app.repo_dlg.open || app.folder_dlg.open || app.unit_dlg.open || app.msg_dlg.open || app.confirm_dlg.open;
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
            if (app.editor.open) app.editor.on_mouse_up();
          }
          break;

        case SDL_EVENT_MOUSE_WHEEL:
          if (app.repo_dlg.open && app.repo_dlg.err_view.mouse_over(app.mx, app.my))
            app.repo_dlg.err_view.on_scroll(ev.wheel.y);
          else if (app.editor.open && !any_dlg())
            app.editor.on_wheel(ev.wheel.y, app.mx, app.my);
          break;

        case SDL_EVENT_TEXT_INPUT:
          if (app.dlg.open)
            app.dlg.fields[app.dlg.focus].handle_text(ev.text.text);
          else if (app.repo_dlg.open)
            app.repo_dlg.fields[app.repo_dlg.focus].handle_text(ev.text.text);
          else if (app.folder_dlg.open)
            app.folder_dlg.name_field.handle_text(ev.text.text);
          else if (app.unit_dlg.open)
            app.unit_dlg.name_field.handle_text(ev.text.text);
          else if (app.editor.editing)
            app.editor.handle_text(ev.text.text);
          break;

        case SDL_EVENT_KEY_DOWN:
          if (app.dlg.open) {
            // ReSharper disable once CppUseStructuredBinding
            for (InputField &f : app.dlg.fields)
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
                  } else if (Conn c = app.dlg.to_conn(); !c.name.empty() && !c.host.empty()) {
                    save_conn(c, app.dlg.old_name);
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
            SDL_Keymod mod      = ev.key.mod;
            bool       consumed = false;
            if (app.repo_dlg.err_view.focused) consumed = app.repo_dlg.err_view.handle_key(ev.key.key, mod);
            if (!consumed) consumed = app.repo_dlg.fields[app.repo_dlg.focus].handle_key(ev.key.key, mod);
            if (!consumed) {
              switch (ev.key.key) {
                case SDLK_TAB: app.repo_dlg.focus = (app.repo_dlg.focus + 1) % 2; break;
                case SDLK_ESCAPE:
                  app.repo_dlg.open = false;
                  SDL_StopTextInput(app.win);
                  break;
                default: break;
              }
            }
          } else if (app.folder_dlg.open) {
            SDL_Keymod mod = ev.key.mod;
            // ReSharper disable once CppTooWideScopeInitStatement
            bool consumed  = app.folder_dlg.name_field.handle_key(ev.key.key, mod);
            if (!consumed && ev.key.key == SDLK_ESCAPE) {
              app.folder_dlg.open = false;
              SDL_StopTextInput(app.win);
            }
          } else if (app.unit_dlg.open) {
            SDL_Keymod mod = ev.key.mod;
            // ReSharper disable once CppTooWideScopeInitStatement
            bool consumed  = app.unit_dlg.name_field.handle_key(ev.key.key, mod);
            if (!consumed && ev.key.key == SDLK_ESCAPE) {
              app.unit_dlg.open = false;
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

    sc(app.ren, C_BG);
    SDL_RenderClear(app.ren);
    panel_render(app.ren, app, l_click, r_click, dblclick);

    float pw = app.ww * 0.30f;
    fill(app.ren, C_BG, pw, 0, app.ww - pw, static_cast<float>(app.wh));

    if (app.editor.open) {
      bool       editor_clicks = !any_dlg();
      static bool prev_editing = false;
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
    }

    if (app.dlg.open) {
      DlgMouse dm;
      dm.mx     = l_click ? l_click_x : (r_click ? r_click_x : app.mx);
      dm.my     = l_click ? l_click_y : (r_click ? r_click_y : app.my);
      dm.ldown  = l_click;
      dm.rdown  = r_click;
      dm.clicks = l_clicks;

      if (int result = dlg_render(app.ren, app.dlg, dm); result == 1) {
        save_conn(app.dlg.to_conn(), app.dlg.old_name);
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
        for (ConnNode &node : app.conns) {
          if (node.conn.name == app.repo_dlg.conn.name) {
            if (!app.repo_dlg.editing) {
              node.conn.connected = true;
              save_conn(node.conn);
            }
            if (node.open) {
              std::vector<RepoNode> repos;
              // ReSharper disable once CppTooWideScopeInitStatement
              auto [ok, err] = connect_and_load(node.conn, repos);
              if (ok) {
                node.repos = std::move(repos);
                restore_open_repos_and_folders(node); // keep already-open repos/folders open
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

          auto [ok, err] = load_repo_tree(node.conn, repo.schema_name, repo);
          if (!ok) {
            app.msg_dlg = {true, "Ошибка", std::move(err)};
          } else {
            restore_repo_folders_open(node.conn.name, repo); // keep already-open folders open
            if (!editing) open_added_folder_parent(node.conn.name, repo, parent_id);
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
          ConnNode         &node      = app.conns[uci];
          RepoNode         &repo      = node.repos[uri];
          const bool        editing   = app.unit_dlg.editing;
          const std::string parent_id = app.unit_dlg.parent_folder_id;

          auto [ok, err] = load_repo_tree(node.conn, repo.schema_name, repo);
          if (!ok) {
            app.msg_dlg = {true, "Ошибка", std::move(err)};
          } else {
            restore_repo_folders_open(node.conn.name, repo);                         // keep already-open folders open
            if (!editing) open_added_folder_parent(node.conn.name, repo, parent_id); // reveal the new unit
          }
        }
        app.unit_dlg.open = false;
        SDL_StopTextInput(app.win);
      } else if (res == -1) {
        app.unit_dlg.open = false;
        SDL_StopTextInput(app.win);
      }
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
            delete_conn(app.conns[idx].conn.name);
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
            auto [ok, err] = delete_folder_recursive(app.conns[fci].conn, repo.schema_name, app.pending_delete_folder_id);
            if (ok) {
              // ReSharper disable once CppTooWideScopeInitStatement
              auto [ok2, err2] = load_repo_tree(app.conns[fci].conn, repo.schema_name, repo);
              if (!ok2)
                app.msg_dlg = {true, "Ошибка", std::move(err2)};
              else
                restore_repo_folders_open(app.conns[fci].conn.name, repo); // keep already-open folders open
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
            auto [ok, err] = delete_unit(app.conns[uci].conn, repo.schema_name, app.pending_delete_unit_id);
            if (ok) {
              // ReSharper disable once CppTooWideScopeInitStatement
              auto [ok2, err2] = load_repo_tree(app.conns[uci].conn, repo.schema_name, repo);
              if (!ok2)
                app.msg_dlg = {true, "Ошибка", std::move(err2)};
              else
                restore_repo_folders_open(app.conns[uci].conn.name, repo); // keep already-open folders open
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

  SDL_DestroyRenderer(app.ren);
  SDL_DestroyWindow(app.win);
  SDL_Quit();
  return 0;
}
