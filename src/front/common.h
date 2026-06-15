#pragma once
#include "App.h"
#include "Clr.h"
#include "Dlg.h"
#include "DlgMouse.h"
#include <SDL3/SDL.h>

namespace front {

  void draw_plus(SDL_Renderer *r, float cx, float cy, float sz, Clr c);
  void draw_pencil(SDL_Renderer *r, float cx, float cy, float sz, Clr c);

  // returns 0=open, 1=saved, -1=cancelled
  int  dlg_render(SDL_Renderer *ren, Dlg &d, const DlgMouse &m);
  void panel_render(SDL_Renderer *ren, App &app, bool click, bool rclick, bool dblclick);

  // Reopen the branches that were open last time (persisted by ProjectTreeService).
  // Call once after the connections are loaded, before the first render.
  void restore_tree_open_state(App &app);

} // namespace front
