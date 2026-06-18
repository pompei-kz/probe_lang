#pragma once
#include "Clr.h"
#include "FontAtlas.h"
#include "render_helpers.h"
#include <SDL3/SDL.h>

namespace front {

  // A form button shared by the edit forms. `primary` fills with the accent when
  // highlighted (used for Save/OK); otherwise it uses the hover fill. The button
  // is highlighted when hovered or keyboard-`focused`, and a light focus ring is
  // drawn when focused so it is obvious that Enter will press it.
  //
  // Returns true if activated this frame: a left click on it (`ldown` while
  // hovered), or Enter while focused (`activate`).
  inline bool form_button(SDL_Renderer *r,
                          float         x,
                          float         y,
                          float         w,
                          float         h,
                          const char   *label,
                          bool          primary,
                          bool          hovered,
                          bool          focused,
                          bool          ldown,
                          bool          activate)
  {
    const bool hl = hovered || focused;
    fill(r, primary ? (hl ? C_ACCENT : C_BORDER) : (hl ? C_HOVER : C_BORDER), x, y, w, h);
    if (focused) rect(r, C_TEXT, x, y, w, h); // focus ring (visible on any fill)
    const Clr tc = (primary && hl) ? C_PANEL : C_TEXT;
    text_draw(r, label, x + (w - text_w(label)) * .5f, center_baseline(y, h), tc);
    return (ldown && hovered) || (activate && focused);
  }

} // namespace front
