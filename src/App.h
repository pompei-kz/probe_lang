#pragma once
#include "Conn.h"
#include "Dlg.h"
#include <SDL3/SDL.h>
#include <vector>

struct App
{
  SDL_Window   *win = nullptr;
  SDL_Renderer *ren = nullptr;
  int           ww = 1280, wh = 720;

  std::vector<Conn> conns;
  Dlg               dlg;

  int   h_item   = -1;
  int   h_edit   = -1;
  bool  h_add    = false;
  float mx       = 0;
  float my       = 0;
  bool  lmb_held = false;
};
