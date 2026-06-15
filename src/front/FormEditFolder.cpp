#include "FormEditFolder.h"
#include "Clr.h"
#include "ContextMenu.h"
#include "FontAtlas.h"
#include "back/FolderService.h"
#include "render_helpers.h"

namespace front {

  using namespace back;
  using namespace back::model;

  static constexpr float FDW   = 380.f;
  static constexpr float FDH   = 210.f;
  static constexpr float FFH   = 28.f;
  static constexpr float ERR_H = 60.f;

  void FormEditFolder::open_add(int ci, int ri, const Conn &c, const std::string &schema, const std::string &parent_id)
  {
    conn_idx            = ci;
    repo_idx            = ri;
    conn                = c;
    schema_name         = schema;
    parent_folder_id    = parent_id;
    editing             = false;
    folder_id           = "";
    name_field.ed       = TextEditor{};
    name_field.ctx.open = false;
    err_view.set("");
  }

  void FormEditFolder::open_edit(int ci, int ri, const Conn &c, const std::string &schema, const std::string &fid, const std::string &fname)
  {
    open_add(ci, ri, c, schema, "");
    folder_id = fid;
    name_field.ed.set(fname);
    editing = true;
  }

  int FormEditFolder::render(SDL_Renderer *ren, float mx, float my, bool ldown, bool rdown, int clicks)
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

    text_draw(ren, editing ? "Изменить папку" : "Добавить папку", dx + 16, dy + 24, C_TEXT);
    sc(ren, C_BORDER);
    SDL_FRect sep{dx + 1, dy + 36, FDW - 2, 1};
    SDL_RenderFillRect(ren, &sep);

    float fw       = FDW - 32;
    float by       = dy + 48 + FS + 6;
    float text_ox  = dx + 16.f + 6.f;
    float clamp_cx = dx + FDW - ContextMenu::W - 4.f;
    float clamp_cy = dy + FDH - ContextMenu::N * ContextMenu::IH - 10.f;

    text_draw(ren, "Имя папки", dx + 16, dy + 48 + FS, C_DIM);
    name_field.draw(ren, dx + 16, by, fw, FFH, true);
    if (ldown) name_field.on_ldown(text_ox, mx, my, dx + 16, by, fw, FFH, clicks);
    if (rdown) name_field.on_rdown(mx, my, dx + 16, by, fw, FFH, clamp_cx, clamp_cy);

    float ev_y = by + FFH + 8;
    err_view.render(ren, dx + 16, ev_y, fw, ERR_H, mx, my, ldown, rdown, C_ERR, clicks);

    name_field.render_ctx(ren, mx, my, ldown, rdown);

    constexpr float BH = 30.f, BW_S = 100.f, BW_C = 80.f;
    float           btn_y = dy + FDH - 50;
    float           sx    = dx + FDW - 16 - BW_S;
    float           cx    = sx - 10 - BW_C;

    bool any_ctx = name_field.ctx.open;
    bool h_save  = !any_ctx && hit(mx, my, sx, btn_y, BW_S, BH);
    bool h_can   = !any_ctx && hit(mx, my, cx, btn_y, BW_C, BH);
    fill(ren, h_save ? C_ACCENT : C_BORDER, sx, btn_y, BW_S, BH);
    fill(ren, h_can ? C_HOVER : C_BORDER, cx, btn_y, BW_C, BH);

    auto btn_t = [&](const char *t, float bx, float bw, Clr c) { text_draw(ren, t, bx + (bw - text_w(t)) * .5f, center_baseline(btn_y, BH), c); };
    btn_t("Сохранить", sx, BW_S, h_save ? C_PANEL : C_TEXT);
    btn_t("Отмена", cx, BW_C, C_TEXT);

    if (ldown && !any_ctx) {
      if (h_can) return -1;
      if (h_save) {
        const std::string &name = name_field.ed.buf;
        if (name.empty()) {
          err_view.set("Имя папки обязательно");
          return 0;
        }
        auto [ok, msg] = editing ? rename_folder(conn, schema_name, folder_id, name) : create_folder(conn, schema_name, parent_folder_id, name);
        if (ok) return 1;
        err_view.set(msg);
      }
    }
    return 0;
  }

} // namespace front
