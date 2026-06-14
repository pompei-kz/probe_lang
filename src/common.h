#pragma once
#include "App.h"
#include "Clr.h"
#include "Dlg.h"
#include "DlgMouse.h"
#include <SDL3/SDL.h>

void draw_plus(SDL_Renderer *r, float cx, float cy, float sz, Clr c);
void draw_pencil(SDL_Renderer *r, float cx, float cy, float sz, Clr c);

// returns 0=open, 1=saved, -1=cancelled
int  dlg_render(SDL_Renderer *ren, Dlg &d, const DlgMouse &m);
void panel_render(SDL_Renderer *ren, App &app, bool click);
