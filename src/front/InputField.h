#pragma once
#include "ContextMenu.h"
#include "TextEditor.h"
#include <SDL3/SDL.h>

namespace front {

  struct InputField
  {
    TextEditor  ed;
    ContextMenu ctx;

    // Draw the field box
    void draw(SDL_Renderer *r, float bx, float by, float bw, float bh, bool focused);

    // Handle left-click on field; returns true if field was hit (caller sets focus)
    bool on_ldown(float text_ox, float mx, float my, float bx, float by, float bw, float bh, int clicks);

    // Handle right-click; opens context menu if field was hit; returns true if hit
    bool on_rdown(float mx, float my, float bx, float by, float bw, float bh, float clamp_max_x, float clamp_max_y);

    // Call every frame while lmb held (TextEditor checks drag state internally)
    void on_move(float mx) { ed.on_mouse_move(mx); }
    void on_release() { ed.on_mouse_release(); }

    bool handle_key(SDL_Keycode key, SDL_Keymod mod) { return ed.handle_key(key, mod); }
    void handle_text(const char *t) { ed.handle_text(t); }

    // Render context menu on top of everything; applies copy/cut/paste to ed
    void render_ctx(SDL_Renderer *r, float mx, float my, bool ldown, bool rdown);
  };

} // namespace front
