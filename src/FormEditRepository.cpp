#include "FormEditRepository.h"
#include "Clr.h"
#include "ConnTest.h"
#include "ContextMenu.h"
#include "FontAtlas.h"
#include "render_helpers.h"

static constexpr float FDW   = 420.f;
static constexpr float FDH   = 320.f;
static constexpr float FFH   = 28.f;
static constexpr float FFS   = 58.f;
static constexpr float ERR_H = 80.f;

void FormEditRepository::open_for(const Conn &c)
{
  for (auto &f : fields) { f.ed = TextEditor{}; f.ctx.open = false; }
  focus           = 0;
  err_view.set("");
  conn            = c;
  editing         = false;
  original_schema = "";
}

void FormEditRepository::open_edit_for(const Conn &c, const SchemaNode &s)
{
  open_for(c);
  fields[0].ed.set(s.schema_name);
  fields[1].ed.set(s.repo_name);
  editing         = true;
  original_schema = s.schema_name;
}

int FormEditRepository::render(SDL_Renderer *ren, float mx, float my, bool ldown, bool rdown, int clicks)
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

  text_draw(ren, editing ? "Edit repository" : "Add repository", dx + 16, dy + 24, C_TEXT);
  sc(ren, C_BORDER);
  SDL_FRect sep{dx + 1, dy + 36, FDW - 2, 1};
  SDL_RenderFillRect(ren, &sep);

  float fw       = FDW - 32, ct = dy + 48;
  float text_ox  = dx + 16.f + 6.f;
  float clamp_cx = dx + FDW - ContextMenu::W - 4.f;
  float clamp_cy = dy + FDH - ContextMenu::N * ContextMenu::IH - 10.f;

  const char *labels[2] = {"Schema name", "Repository name"};
  for (int i = 0; i < 2; i++) {
    float fy = ct + i * FFS;
    float by = fy + FS + 6;
    text_draw(ren, labels[i], dx + 16, fy + FS, C_DIM);
    fields[i].draw(ren, dx + 16, by, fw, FFH, focus == i);

    if (ldown) {
      if (fields[i].on_ldown(text_ox, mx, my, dx + 16, by, fw, FFH, clicks)) {
        focus            = i;
        err_view.focused = false;
      }
    }
    if (rdown) {
      if (fields[i].on_rdown(mx, my, dx + 16, by, fw, FFH, clamp_cx, clamp_cy))
        focus = i;
    }
  }

  // error text view
  float ev_y = ct + 2 * FFS + 6;
  err_view.render(ren, dx + 16, ev_y, fw, ERR_H, mx, my, ldown, rdown, C_ERR, clicks);

  // context menus rendered on top of fields
  for (auto &f : fields) f.render_ctx(ren, mx, my, ldown, rdown);

  constexpr float BH = 30.f, BW_S = 80.f, BW_C = 80.f;
  float           btn_y = dy + FDH - 50;
  float           sx    = dx + FDW - 16 - BW_S;
  float           cx    = sx - 10 - BW_C;

  bool any_ctx = fields[0].ctx.open || fields[1].ctx.open;
  bool h_save  = !any_ctx && hit(mx, my, sx, btn_y, BW_S, BH);
  bool h_can   = !any_ctx && hit(mx, my, cx, btn_y, BW_C, BH);
  fill(ren, h_save ? C_ACCENT : C_BORDER, sx, btn_y, BW_S, BH);
  fill(ren, h_can  ? C_HOVER  : C_BORDER, cx, btn_y, BW_C, BH);

  auto btn_t = [&](const char *t, float bx, float bw, Clr c) {
    text_draw(ren, t, bx + (bw - text_w(t)) * .5f, center_baseline(btn_y, BH), c);
  };
  btn_t("Save",   sx, BW_S, h_save ? C_PANEL : C_TEXT);
  btn_t("Cancel", cx, BW_C, C_TEXT);

  if (ldown && !any_ctx) {
    if (h_can) return -1;
    if (h_save) {
      const std::string &schema   = fields[0].ed.buf;
      const std::string &reponame = fields[1].ed.buf;
      if (schema.empty())   { err_view.set("Schema name is required"); return 0; }
      if (reponame.empty()) { err_view.set("Repository name is required"); return 0; }
      auto [ok, msg] = editing
          ? edit_repository(conn, original_schema, schema, reponame)
          : create_repository(conn, schema, reponame);
      if (ok) return 1;
      err_view.set(msg);
    }
  }
  return 0;
}
