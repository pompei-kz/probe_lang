#include "InputField.h"
#include "render_helpers.h"
#include <SDL3/SDL.h>

namespace front {

  void InputField::draw(SDL_Renderer *r, float bx, float by, float bw, float bh, bool focused)
  {
    ed.draw(r, bx, by, bw, bh, focused);
  }

  bool InputField::on_ldown(float text_ox, float mx, float my, float bx, float by, float bw, float bh, int clicks)
  {
    if (!hit(mx, my, bx, by, bw, bh)) return false;
    if (ctx.open) return false;
    bool shift = (SDL_GetModState() & SDL_KMOD_SHIFT) != 0;
    ed.on_mouse_press(text_ox, mx, clicks, shift);
    return true;
  }

  bool InputField::on_rdown(float mx, float my, float bx, float by, float bw, float bh, float clamp_max_x, float clamp_max_y)
  {
    if (!hit(mx, my, bx, by, bw, bh)) return false;
    ctx = ContextMenu{true, std::min(mx, clamp_max_x), std::min(my, clamp_max_y), 0};
    return true;
  }

  void InputField::render_ctx(SDL_Renderer *r, float mx, float my, bool ldown, bool rdown)
  {
    int act = ctx.render(r, mx, my, ldown, rdown);
    if (act == 0)
      ed.do_copy();
    else if (act == 1)
      ed.do_cut();
    else if (act == 2)
      ed.do_paste();
  }

} // namespace front
