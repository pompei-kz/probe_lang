#include "FormEditUnit.h"
#include "Clr.h"
#include "ContextMenu.h"
#include "FontAtlas.h"
#include "FormWidgets.h"
#include "back/service/UnitServiceRW.h"
#include "render_helpers.h"

namespace front {

  static constexpr float FDW   = 380.f;
  static constexpr float FDH   = 290.f;
  static constexpr float FFH   = 28.f;
  static constexpr float ERR_H = 60.f;
  static constexpr float DD_IH = 26.f; // type dropdown item height

  void FormEditUnit::open_add(int ci, int ri, const back::model::ConnStore &c, const std::string &schema, const std::string &parent_id)
  {
    conn_idx            = ci;
    repo_idx            = ri;
    conn                = c;
    schema_name         = schema;
    parent_folder_id    = parent_id;
    editing             = false;
    unit_id             = "";
    type                = back::model::UnitType::Class;
    type_dropdown_open  = false;
    name_field.ed       = TextEditor{};
    name_field.ctx.open = false;
    err_view.set("");
    focus    = 0;
    activate = false;
  }

  void FormEditUnit::open_edit(
      int ci, int ri, const back::model::ConnStore &c, const std::string &schema, const std::string &uid, const std::string &uname, back::model::UnitType utype)
  {
    open_add(ci, ri, c, schema, "");
    unit_id = uid;
    name_field.ed.set(uname);
    type    = utype;
    editing = true;
  }

  int FormEditUnit::render(SDL_Renderer *ren, float mx, float my, bool ldown, bool rdown, int clicks)
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

    text_draw(ren, editing ? "Изменить юнит" : "Добавить юнит", dx + 16, dy + 24, C_TEXT);
    sc(ren, C_BORDER);
    SDL_FRect sep{dx + 1, dy + 36, FDW - 2, 1};
    SDL_RenderFillRect(ren, &sep);

    float fw       = FDW - 32;
    float text_ox  = dx + 16.f + 6.f;
    float clamp_cx = dx + FDW - ContextMenu::W - 4.f;
    float clamp_cy = dy + FDH - ContextMenu::N * ContextMenu::IH - 10.f;

    // ── Type combobox ──────────────────────────────────────────────────────
    float ty_lbl_y = dy + 48;
    text_draw(ren, "Тип юнита", dx + 16, ty_lbl_y + FS, C_DIM);
    float cby = ty_lbl_y + FS + 6;
    fill(ren, C_PANEL, dx + 16, cby, fw, FFH);
    rect(ren, focus == COMBO ? C_TEXT : C_BORDER, dx + 16, cby, fw, FFH); // focus ring
    text_draw(ren, to_string(type), dx + 22, center_baseline(cby, FFH), C_TEXT);
    {
      float cxr = dx + 16 + fw - 16, cyr = cby + FFH * .5f;
      sc(ren, C_DIM);
      SDL_RenderLine(ren, cxr - 5, cyr - 2, cxr + 5, cyr - 2);
      SDL_RenderLine(ren, cxr - 5, cyr - 2, cxr, cyr + 3);
      SDL_RenderLine(ren, cxr + 5, cyr - 2, cxr, cyr + 3);
    }
    bool h_combo = hit(mx, my, dx + 16, cby, fw, FFH);

    // ── Name field ─────────────────────────────────────────────────────────
    float ny_lbl_y = ty_lbl_y + 58;
    text_draw(ren, "Имя", dx + 16, ny_lbl_y + FS, C_DIM);
    float by = ny_lbl_y + FS + 6;
    name_field.draw(ren, dx + 16, by, fw, FFH, focus == 0 && !type_dropdown_open);
    if (ldown && !type_dropdown_open && name_field.on_ldown(text_ox, mx, my, dx + 16, by, fw, FFH, clicks)) focus = 0;
    if (rdown) name_field.on_rdown(mx, my, dx + 16, by, fw, FFH, clamp_cx, clamp_cy);

    // ── Error view ─────────────────────────────────────────────────────────
    float ev_y = by + FFH + 8;
    err_view.render(ren, dx + 16, ev_y, fw, ERR_H, mx, my, ldown, rdown, C_ERR, clicks);

    name_field.render_ctx(ren, mx, my, ldown, rdown);

    // ── Type dropdown (rendered on top) ────────────────────────────────────
    bool ate = false;
    if (type_dropdown_open) {
      const back::model::UnitType opts[3] = {back::model::UnitType::Class, back::model::UnitType::Interface, back::model::UnitType::Enum};
      float          dd_x = dx + 16, dd_y = cby + FFH, dd_w = fw, dd_h = 3 * DD_IH;
      fill(ren, C_DLGBG, dd_x, dd_y, dd_w, dd_h);
      rect(ren, C_BORDER, dd_x, dd_y, dd_w, dd_h);
      for (int i = 0; i < 3; i++) {
        float oy  = dd_y + i * DD_IH;
        bool  hov = hit(mx, my, dd_x, oy, dd_w, DD_IH);
        if (hov) fill(ren, C_HOVER, dd_x + 1, oy, dd_w - 2, DD_IH);
        text_draw(ren, to_string(opts[i]), dd_x + 8, center_baseline(oy, DD_IH), C_TEXT);
        if (ldown && hov) {
          type               = opts[i];
          type_dropdown_open = false;
          ate                = true;
        }
      }
      if (ldown && !ate) { // click outside the options closes the dropdown
        type_dropdown_open = false;
        ate                = true;
      }
    } else if (ldown && h_combo) {
      type_dropdown_open = true;
      focus              = COMBO;
      ate                = true;
    }

    // ── Save / Cancel buttons + actions ────────────────────────────────────
    constexpr float BH = 30.f, BW_S = 100.f, BW_C = 80.f;
    const float     btn_y     = dy + FDH - 50;
    const float     sx        = dx + FDW - 16 - BW_S;
    const float     cx        = sx - 10 - BW_C;
    const bool      blocked   = type_dropdown_open || name_field.ctx.open;
    const bool      clickable = ldown && !blocked && !ate;
    const bool      act       = activate && !blocked; // OK is "active" only when not blocked
    activate                  = false;
    const bool do_save =
        form_button(ren, sx, btn_y, BW_S, BH, "Сохранить", true, !blocked && hit(mx, my, sx, btn_y, BW_S, BH), focus == SAVE, clickable, act);
    const bool do_can =
        form_button(ren, cx, btn_y, BW_C, BH, "Отмена", false, !blocked && hit(mx, my, cx, btn_y, BW_C, BH), focus == CANCEL, clickable, act);

    if (do_can) return -1;
    if (do_save) {
      const std::string &name = name_field.ed.buf;
      if (name.empty()) {
        err_view.set("Имя юнита обязательно");
        return 0;
      }
      auto [ok, msg] = editing ? back::edit_unit(conn, schema_name, unit_id, name, type) : back::create_unit(conn, schema_name, parent_folder_id, name, type);
      if (ok) return 1;
      err_view.set(msg);
    }
    return 0;
  }

} // namespace front
