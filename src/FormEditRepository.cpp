#include "FormEditRepository.h"
#include "Clr.h"
#include "ConnTest.h"
#include "FontAtlas.h"
#include "render_helpers.h"

// FDH increased to make room for the error text view
static constexpr float FDW    = 420.f;
static constexpr float FDH    = 320.f;
static constexpr float FFH    = 28.f;
static constexpr float FFS    = 58.f;
static constexpr float ERR_H  = 80.f;  // height of the error text view

void FormEditRepository::open_for(const Conn &c)
{
  editors[0] = TextEditor{};
  editors[1] = TextEditor{};
  focus      = 0;
  err_view.set("");
  conn       = c;
}

int FormEditRepository::render(SDL_Renderer *ren, float mx, float my, bool ldown, bool rdown, int /*clicks*/)
{
  if (!open) return 0;

  int ww, wh;
  SDL_GetCurrentRenderOutputSize(ren, &ww, &wh);

  SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
  fill(ren, C_OVL, 0, 0, (float)ww, (float)wh);
  SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_NONE);

  float dx = (ww - FDW) * .5f, dy = (wh - FDH) * .5f;
  fill(ren, C_DLGBG, dx, dy, FDW, FDH);
  rect(ren, C_BORDER, dx, dy, FDW, FDH);

  text_draw(ren, "Add repository", dx + 16, dy + 24, C_TEXT);
  sc(ren, C_BORDER);
  SDL_FRect sep{dx + 1, dy + 36, FDW - 2, 1};
  SDL_RenderFillRect(ren, &sep);

  float fw = FDW - 32, ct = dy + 48;

  const char *labels[2] = {"Schema name", "Repository name"};
  for (int i = 0; i < 2; i++) {
    float fy = ct + i * FFS;
    float by = fy + FS + 6;
    text_draw(ren, labels[i], dx + 16, fy + FS, C_DIM);
    editors[i].draw(ren, dx + 16, by, fw, FFH, focus == i);
    if (ldown && hit(mx, my, dx + 16, by, fw, FFH)) {
      focus = i;
      err_view.focused = false;
      editors[i].on_mouse_press(dx + 16 + 6, mx, 1, false);
    }
  }

  // error text view — always at fixed position below fields
  float ev_y = ct + 2 * FFS + 6;
  err_view.render(ren, dx + 16, ev_y, fw, ERR_H, mx, my, ldown, rdown, C_ERR);

  constexpr float BH = 30.f, BW_S = 80.f, BW_C = 80.f;
  float           btn_y = dy + FDH - 50;
  float           sx    = dx + FDW - 16 - BW_S;
  float           cx    = sx - 10 - BW_C;

  bool h_save = hit(mx, my, sx, btn_y, BW_S, BH);
  bool h_can  = hit(mx, my, cx, btn_y, BW_C, BH);
  fill(ren, h_save ? C_ACCENT : C_BORDER, sx, btn_y, BW_S, BH);
  fill(ren, h_can  ? C_HOVER  : C_BORDER, cx, btn_y, BW_C, BH);

  auto btn_t = [&](const char *t, float bx, float bw, Clr c) {
    text_draw(ren, t, bx + (bw - text_w(t)) * .5f, center_baseline(btn_y, BH), c);
  };
  btn_t("Save",   sx, BW_S, h_save ? C_PANEL : C_TEXT);
  btn_t("Cancel", cx, BW_C, C_TEXT);

  if (ldown) {
    if (h_can) return -1;
    if (h_save) {
      const std::string &schema   = editors[0].buf;
      const std::string &reponame = editors[1].buf;
      if (schema.empty())   { err_view.set("Schema name is required"); return 0; }
      if (reponame.empty()) { err_view.set("Repository name is required"); return 0; }
      auto [ok, msg] = create_repository(conn, schema, reponame);
      if (ok) return 1;
      err_view.set(msg);
    }
  }
  return 0;
}
