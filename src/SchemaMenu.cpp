#include "SchemaMenu.h"
#include "Clr.h"
#include "FontAtlas.h"
#include "render_helpers.h"

int SchemaMenu::render(SDL_Renderer *r, float mx, float my, bool ldown, bool rdown)
{
  if (!open) return -1;
  float h = N * IH + 4.f;
  if ((ldown || rdown) && !hit(mx, my, x, y, W, h)) { open = false; return -1; }
  fill(r, C_DLGBG, x, y, W, h);
  rect(r, C_BORDER, x, y, W, h);
  float iy  = y + 2.f;
  bool  hov = hit(mx, my, x, iy, W, IH);
  if (hov) fill(r, C_HOVER, x + 1.f, iy, W - 2.f, IH);
  text_draw(r, "Изменить схему", x + 12.f, center_baseline(iy, IH), C_TEXT);
  if (ldown && hov) { open = false; return 0; }
  return -1;
}
