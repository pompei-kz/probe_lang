#include "ConfirmDlg.h"
#include "Clr.h"
#include "FontAtlas.h"
#include "render_helpers.h"

namespace front {

  int ConfirmDlg::render(SDL_Renderer *ren, float mx, float my, bool ldown)
  {
    if (!open) return 0;

    int ww, wh;
    SDL_GetCurrentRenderOutputSize(ren, &ww, &wh);

    constexpr float DW = 380.f, DH = 150.f, BW = 80.f, BH = 30.f, GAP = 12.f;
    float           dx = (ww - DW) * .5f, dy = (wh - DH) * .5f;

    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
    fill(ren, C_OVL, 0, 0, (float)ww, (float)wh);
    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_NONE);

    fill(ren, C_DLGBG, dx, dy, DW, DH);
    rect(ren, C_BORDER, dx, dy, DW, DH);

    text_draw(ren, title.empty() ? "Подтверждение" : title.c_str(), dx + 16, dy + 24, C_TEXT);
    sc(ren, C_BORDER);
    SDL_FRect sep{dx + 1, dy + 36, DW - 2, 1};
    SDL_RenderFillRect(ren, &sep);

    text_draw(ren, msg.c_str(), dx + 16, dy + 36 + FS + 10, C_TEXT);

    float total_bw = BW * 2 + GAP;
    float bx_yes   = dx + (DW - total_bw) * .5f;
    float bx_no    = bx_yes + BW + GAP;
    float by       = dy + DH - 46;

    bool h_yes = hit(mx, my, bx_yes, by, BW, BH);
    bool h_no  = hit(mx, my, bx_no, by, BW, BH);

    fill(ren, h_yes ? C_ACCENT : C_BORDER, bx_yes, by, BW, BH);
    fill(ren, h_no ? C_HOVER : C_BORDER, bx_no, by, BW, BH);

    text_draw(ren, "Да", bx_yes + (BW - text_w("Да")) * .5f, center_baseline(by, BH), h_yes ? C_PANEL : C_TEXT);
    text_draw(ren, "Нет", bx_no + (BW - text_w("Нет")) * .5f, center_baseline(by, BH), C_TEXT);

    if (ldown) {
      if (h_yes) {
        open = false;
        return 1;
      }
      if (h_no) {
        open = false;
        return -1;
      }
    }
    return 0;
  }

} // namespace front
