#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include "App.h"
#include "Conn.h"
#include "DlgMouse.h"
#include "FontAtlas.h"
#include "common.h"
#include "render_helpers.h"

int main(int /*argc*/, char * /*argv*/[])
{
  if (!SDL_Init(SDL_INIT_VIDEO)) {
    SDL_Log("SDL_Init: %s", SDL_GetError());
    return 1;
  }

  App app;
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
  app.conns = load_all();

  if (SDL_GetEnvironmentVariable(SDL_GetEnvironment(), "PROBE_TEST_DIALOG")) {
    app.dlg.open_add();
    app.dlg.open = true;
    SDL_StartTextInput(app.win);
  } else if (SDL_GetEnvironmentVariable(SDL_GetEnvironment(), "PROBE_TEST_EDIT") && !app.conns.empty()) {
    app.dlg.open_edit(app.conns[0]);
    app.dlg.open = true;
    SDL_StartTextInput(app.win);
  }

  bool running = true;
  while (running) {
    bool  lclick = false, rclick = false;
    float lclick_x = 0, lclick_y = 0, rclick_x = 0, rclick_y = 0;
    int   lclicks = 1;

    SDL_Event ev;
    while (SDL_PollEvent(&ev)) {
      switch (ev.type) {
        case SDL_EVENT_QUIT: running = false; break;

        case SDL_EVENT_WINDOW_RESIZED: SDL_GetWindowSize(app.win, &app.ww, &app.wh); break;

        case SDL_EVENT_MOUSE_MOTION:
          app.mx = ev.motion.x;
          app.my = ev.motion.y;
          if (app.dlg.open && app.dlg.active_drag_ed >= 0 && app.lmb_held) app.dlg.editors[app.dlg.active_drag_ed].on_mouse_move(ev.motion.x);
          break;

        case SDL_EVENT_MOUSE_BUTTON_DOWN:
          if (ev.button.button == SDL_BUTTON_LEFT) {
            app.lmb_held = true;
            lclick       = true;
            lclick_x     = ev.button.x;
            lclick_y     = ev.button.y;
            lclicks      = ev.button.clicks;
          }
          if (ev.button.button == SDL_BUTTON_RIGHT) {
            rclick   = true;
            rclick_x = ev.button.x;
            rclick_y = ev.button.y;
          }
          break;

        case SDL_EVENT_MOUSE_BUTTON_UP:
          if (ev.button.button == SDL_BUTTON_LEFT) {
            app.lmb_held = false;
            if (app.dlg.open && app.dlg.active_drag_ed >= 0) {
              app.dlg.editors[app.dlg.active_drag_ed].on_mouse_release();
              app.dlg.active_drag_ed = -1;
            }
          }
          break;

        case SDL_EVENT_TEXT_INPUT:
          if (app.dlg.open) app.dlg.editors[app.dlg.focus].handle_text(ev.text.text);
          break;

        case SDL_EVENT_KEY_DOWN:
          if (app.dlg.open) {
            app.dlg.ctx_menu.open = false;
            SDL_Keymod mod        = ev.key.mod;
            bool       consumed   = app.dlg.editors[app.dlg.focus].handle_key(ev.key.key, mod);
            if (!consumed) {
              bool shift = (mod & SDL_KMOD_SHIFT) != 0;
              switch (ev.key.key) {
                case SDLK_TAB: app.dlg.focus = (app.dlg.focus + (shift ? 4 : 1)) % 5; break;
                case SDLK_ESCAPE:
                  app.dlg.open = false;
                  SDL_StopTextInput(app.win);
                  break;
                case SDLK_RETURN:
                case SDLK_KP_ENTER:
                  if (app.dlg.focus < 4) {
                    app.dlg.focus++;
                  } else if (Conn c = app.dlg.to_conn(); !c.name.empty() && !c.host.empty()) {
                    save_conn(c, app.dlg.old_name);
                    app.conns    = load_all();
                    app.dlg.open = false;
                    SDL_StopTextInput(app.win);
                  } else {
                    app.dlg.err = c.name.empty() ? "Name is required" : "Host is required";
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

    sc(app.ren, C_BG);
    SDL_RenderClear(app.ren);
    panel_render(app.ren, app, lclick, rclick);

    float pw = app.ww * 0.30f;
    fill(app.ren, C_BG, pw, 0, app.ww - pw, (float)app.wh);

    if (app.dlg.open) {
      DlgMouse dm;
      dm.mx     = lclick ? lclick_x : (rclick ? rclick_x : app.mx);
      dm.my     = lclick ? lclick_y : (rclick ? rclick_y : app.my);
      dm.ldown  = lclick;
      dm.rdown  = rclick;
      dm.clicks = lclicks;

      if (int result = dlg_render(app.ren, app.dlg, dm); result == 1) {
        save_conn(app.dlg.to_conn(), app.dlg.old_name);
        app.conns    = load_all();
        app.dlg.open = false;
        SDL_StopTextInput(app.win);
      } else if (result == -1) {
        app.dlg.open = false;
        SDL_StopTextInput(app.win);
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
