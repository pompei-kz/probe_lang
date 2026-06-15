#include "FolderMenu.h"
#include "Clr.h"
#include "FontAtlas.h"
#include "render_helpers.h"

namespace front {

  int FolderMenu::render(SDL_Renderer *r, float mx, float my, bool ldown, bool rdown)
  {
    if (!open) return -1;
    if (pending >= 0) {
      int a   = pending;
      pending = -1;
      open    = false;
      return a;
    }
    float h = N * IH + 4.f;
    if ((ldown || rdown) && !hit(mx, my, x, y, W, h)) {
      open = false;
      return -1;
    }
    fill(r, C_DLGBG, x, y, W, h);
    rect(r, C_BORDER, x, y, W, h);

    static const char *labels[N] = {"Добавить юнит", "Добавить папку", "Изменить эту папку", "Удалить эту папку"};
    for (int i = 0; i < N; i++) {
      float iy  = y + 2.f + i * IH;
      bool  hov = hit(mx, my, x, iy, W, IH);
      if (hov || i == hi) fill(r, C_HOVER, x + 1.f, iy, W - 2.f, IH);
      text_draw(r, labels[i], x + 12.f, center_baseline(iy, IH), C_TEXT);
      if (ldown && hov) {
        open = false;
        return i;
      }
    }
    return -1;
  }

} // namespace front
