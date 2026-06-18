#include "ContextMenuSelExpr.h"
#include "Clr.h"
#include "FontAtlas.h"
#include "render_helpers.h"
#include <algorithm>

namespace front {

  namespace {
    constexpr float IH   = 24.f; // item height
    constexpr float PADX = 10.f; // text inset
    constexpr float ARROW = 14.f; // right gutter for the ▶ submenu indicator

    // Width of a menu box fitting the widest of `labels`, plus a ▶ gutter when
    // any item has a submenu.
    float box_width(const char *const *labels, int n, bool with_arrow)
    {
      float w = 60.f;
      for (int i = 0; i < n; i++) w = std::max(w, PADX + text_w(labels[i]) + PADX + (with_arrow ? ARROW : 0.f));
      return w;
    }

    // Draw one menu box and return the hovered item index (-1 if none). Draws a
    // ▶ on the item at `arrow_row` (or -1 for none).
    int draw_box(SDL_Renderer *ren, float x, float y, float w, const char *const *labels, int n, float mx, float my, int arrow_row)
    {
      const float h = n * IH + 4.f;
      fill(ren, C_DLGBG, x, y, w, h);
      rect(ren, C_BORDER, x, y, w, h);
      int hov = -1;
      for (int i = 0; i < n; i++) {
        const float iy = y + 2.f + i * IH;
        if (hit(mx, my, x, iy, w, IH)) {
          hov = i;
          fill(ren, C_HOVER, x + 1.f, iy, w - 2.f, IH);
        }
        text_draw(ren, labels[i], x + PADX, center_baseline(iy, IH), C_TEXT);
        if (i == arrow_row) text_draw(ren, ">", x + w - ARROW, center_baseline(iy, IH), C_DIM);
      }
      return hov;
    }
  } // namespace

  void ContextMenuSelExpr::open_at(float ax, float ay, const std::string &fid)
  {
    open         = true;
    x            = ax;
    y            = ay;
    field_id     = fid;
    submenu_open = false;
    just_opened  = true;
  }

  void ContextMenuSelExpr::close() { open = false; }

  ContextMenuSelExpr::Action ContextMenuSelExpr::render(SDL_Renderer *ren, float mx, float my, bool ldown)
  {
    if (!open) return Action::None;

    // Ignore the press that opened the menu (and any lingering second press of a
    // double-click) for the first frame, so it can't immediately close itself.
    const bool click = ldown && !just_opened;
    just_opened      = false;

    static const char *L1[] = {"Этот", "Юнит"};
    static const char *L2[] = {"Объект", "Юнит", "Метод"};
    constexpr int      N1 = 2, N2 = 3;

    const float w1 = box_width(L1, N1, true);
    const float h1 = N1 * IH + 4.f;
    const float w2 = box_width(L2, N2, false);

    // Submenu opens to the right of the "Этот" row (row 0).
    const float sub_x = x + w1 - 1.f;
    const float sub_y = y;
    const float h2    = N2 * IH + 4.f;

    // A press outside both boxes dismisses the whole menu.
    const bool over_main = hit(mx, my, x, y, w1, h1);
    const bool over_sub  = submenu_open && hit(mx, my, sub_x, sub_y, w2, h2);
    if (click && !over_main && !over_sub) {
      close();
      return Action::None;
    }

    const int hov1 = draw_box(ren, x, y, w1, L1, N1, mx, my, /*arrow_row=*/0);
    // Hovering "Этот" expands the submenu; hovering the other level-1 item folds it.
    if (hov1 == 0)
      submenu_open = true;
    else if (hov1 == 1)
      submenu_open = false;

    int hov2 = -1;
    if (submenu_open) hov2 = draw_box(ren, sub_x, sub_y, w2, L2, N2, mx, my, /*arrow_row=*/-1);

    if (click) {
      if (hov2 == 0) {
        close();
        return Action::SetThisObject;
      }
      if (hov2 == 1) {
        close();
        return Action::SetThisUnit;
      }
      if (hov2 == 2) {
        close();
        return Action::SetThisMethod;
      }
      if (hov1 == 1) {
        close();
        return Action::OpenUnitForm;
      }
    }
    return Action::None;
  }

} // namespace front
