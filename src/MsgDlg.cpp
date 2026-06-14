#include "MsgDlg.h"
#include "Clr.h"
#include "FontAtlas.h"
#include "render_helpers.h"

bool MsgDlg::render(SDL_Renderer *ren, float mx, float my, bool ldown)
{
  if (!open) return false;

  int ww, wh;
  SDL_GetCurrentRenderOutputSize(ren, &ww, &wh);

  constexpr float DW = 420.f, DH = 220.f, BW = 80.f, BH = 30.f;
  float           dx = (ww - DW) * .5f, dy = (wh - DH) * .5f;

  SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
  fill(ren, C_OVL, 0, 0, (float)ww, (float)wh);
  SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_NONE);

  fill(ren, C_DLGBG, dx, dy, DW, DH);
  rect(ren, C_BORDER, dx, dy, DW, DH);

  text_draw(ren, title.empty() ? "Error" : title.c_str(), dx + 16, dy + 24, C_ERR);
  sc(ren, C_BORDER);
  SDL_FRect sep{dx + 1, dy + 36, DW - 2, 1};
  SDL_RenderFillRect(ren, &sep);

  // show up to 6 lines, splitting on '\n'
  float  y_off  = dy + 52;
  size_t start  = 0;
  int    nlines = 0;
  while (start <= msg.size() && nlines < 6) {
    size_t end = msg.find('\n', start);
    if (end == std::string::npos) end = msg.size();
    SDL_Rect clip{(int)(dx + 16), (int)y_off, (int)(DW - 32), (int)(FS + 4)};
    SDL_SetRenderClipRect(ren, &clip);
    text_draw(ren, msg.substr(start, end - start).c_str(), dx + 16, y_off + FS, C_TEXT);
    SDL_SetRenderClipRect(ren, nullptr);
    y_off += FS + 4;
    nlines++;
    start = end + 1;
    if (end == msg.size()) break;
  }

  float bx = dx + (DW - BW) * .5f, by = dy + DH - 46;
  bool  hok = hit(mx, my, bx, by, BW, BH);
  fill(ren, hok ? C_HOVER : C_BORDER, bx, by, BW, BH);
  text_draw(ren, "OK", bx + (BW - text_w("OK")) * .5f, center_baseline(by, BH), C_TEXT);

  return ldown && hok;
}
