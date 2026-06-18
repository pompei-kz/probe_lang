#include "FormSelectUnit.h"
#include "Clr.h"
#include "ContextMenu.h"
#include "FontAtlas.h"
#include "FormWidgets.h"
#include "back/ExprService.h"
#include "back/UnitService.h"
#include "render_helpers.h"
#include <algorithm>

namespace front {

  using namespace back;
  using namespace back::model;

  namespace {
    constexpr float FDW = 420.f, FDH = 480.f;
    constexpr float FFH  = 28.f; // filter field height
    constexpr float ROW_H = 26.f;
    constexpr float PADY  = 4.f;
    constexpr int   PAGE  = 40; // units fetched per page
  } // namespace

  void FormSelectUnit::open_for(const Conn &c, const std::string &sch, const std::string &fid)
  {
    open         = true;
    conn         = c;
    schema       = sch;
    field_id     = fid;
    filter_field = InputField{};
    focus        = 0;
    activate     = false;
    scroll_y     = 0.f;
    reset_for_filter("");
  }

  void FormSelectUnit::close() { open = false; }

  void FormSelectUnit::reset_for_filter(const std::string &filter)
  {
    applied_filter = filter;
    units.clear();
    next_offset = 0;
    has_more    = true;
    scroll_y    = 0.f;
    load_next_page();
  }

  void FormSelectUnit::load_next_page()
  {
    if (!has_more) return;
    auto [page, err] = list_units_paginated(conn, schema, applied_filter, next_offset, PAGE);
    next_offset += static_cast<int>(page.size());
    if (static_cast<int>(page.size()) < PAGE) has_more = false;
    for (Unit &u : page) units.push_back(std::move(u));
  }

  // Total content height: one row per unit, plus the "no more data" line.
  static float content_height(int n_units, bool has_more) { return (n_units + (has_more ? 0 : 1)) * ROW_H; }

  void FormSelectUnit::on_scroll(float dy)
  {
    const float content = content_height(static_cast<int>(units.size()), has_more);
    const float inner   = std::max(1.f, list_h - 2.f * PADY);
    const float max_s   = std::max(0.f, content - inner);
    scroll_y            = std::clamp(scroll_y - dy * ROW_H * 3.f, 0.f, max_s);
  }

  bool FormSelectUnit::mouse_over_list(float mx, float my) const { return hit(mx, my, list_x, list_y, list_w, list_h); }

  int FormSelectUnit::render(SDL_Renderer *ren, float mx, float my, bool ldown, bool rdown, int clicks)
  {
    if (!open) return 0;

    // Re-fetch from scratch whenever the filter text changes.
    if (filter_field.ed.buf != applied_filter) reset_for_filter(filter_field.ed.buf);

    int ww, wh;
    SDL_GetCurrentRenderOutputSize(ren, &ww, &wh);

    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
    fill(ren, C_OVL, 0, 0, (float)ww, (float)wh);
    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_NONE);

    const float dx = (ww - FDW) * .5f, dy = (wh - FDH) * .5f;
    fill(ren, C_DLGBG, dx, dy, FDW, FDH);
    rect(ren, C_BORDER, dx, dy, FDW, FDH);

    text_draw(ren, "Выбор юнита", dx + 16, dy + 24, C_TEXT);
    sc(ren, C_BORDER);
    SDL_FRect sep{dx + 1, dy + 36, FDW - 2, 1};
    SDL_RenderFillRect(ren, &sep);

    const float fw       = FDW - 32;
    const float text_ox  = dx + 16.f + 6.f;
    const float clamp_cx = dx + FDW - ContextMenu::W - 4.f;
    const float clamp_cy = dy + FDH - ContextMenu::N * ContextMenu::IH - 10.f;

    // ── Filter field ──────────────────────────────────────────────────────────
    const float fy = dy + 48;
    filter_field.draw(ren, dx + 16, fy, fw, FFH, focus == 0);
    if (ldown && filter_field.on_ldown(text_ox, mx, my, dx + 16, fy, fw, FFH, clicks)) focus = 0;
    if (rdown) filter_field.on_rdown(mx, my, dx + 16, fy, fw, FFH, clamp_cx, clamp_cy);

    // ── Buttons ────────────────────────────────────────────────────────────────
    constexpr float BH = 30.f, BW_C = 90.f;
    const float     btn_y = dy + FDH - 16 - BH;
    const float     cxb   = dx + FDW - 16 - BW_C;
    const bool      act   = activate;
    activate              = false;
    const bool do_can =
        form_button(ren, cxb, btn_y, BW_C, BH, "Отмена", false, !filter_field.ctx.open && hit(mx, my, cxb, btn_y, BW_C, BH), focus == CANCEL, ldown, act);

    // ── Unit list ───────────────────────────────────────────────────────────────
    list_x = dx + 16;
    list_y = fy + FFH + 10;
    list_w = fw;
    list_h = btn_y - 10 - list_y;
    fill(ren, C_INBG, list_x, list_y, list_w, list_h);
    rect(ren, C_BORDER, list_x, list_y, list_w, list_h);

    // Lazily append pages while the bottom is in view.
    const float inner = std::max(1.f, list_h - 2.f * PADY);
    if (has_more && scroll_y + inner >= content_height(static_cast<int>(units.size()), has_more) - ROW_H) load_next_page();

    int chosen = -1;
    {
      SDL_Rect clip{(int)(list_x + 1), (int)(list_y + 1), (int)(list_w - 2), (int)(list_h - 2)};
      SDL_SetRenderClipRect(ren, &clip);
      const bool ctx_open = filter_field.ctx.open;
      for (int i = 0; i < static_cast<int>(units.size()); i++) {
        const float ry = list_y + PADY + i * ROW_H - scroll_y;
        if (ry + ROW_H < list_y || ry > list_y + list_h) continue; // offscreen
        const bool hov = !ctx_open && hit(mx, my, list_x, ry, list_w, ROW_H) && hit(mx, my, list_x, list_y, list_w, list_h);
        if (hov) fill(ren, C_HOVER, list_x + 1.f, ry, list_w - 2.f, ROW_H);
        text_draw(ren, units[i].name.c_str(), list_x + 10.f, center_baseline(ry, ROW_H), C_TEXT);
        if (ldown && hov) chosen = i;
      }
      if (!has_more) {
        const float ry = list_y + PADY + static_cast<int>(units.size()) * ROW_H - scroll_y;
        if (ry + ROW_H >= list_y && ry <= list_y + list_h)
          text_draw(ren, "(Всё, данных больше нет)", list_x + 10.f, center_baseline(ry, ROW_H), C_DIM);
      }
      SDL_SetRenderClipRect(ren, nullptr);
    }

    // Scrollbar thumb.
    const float content = content_height(static_cast<int>(units.size()), has_more);
    if (content > inner + 0.5f) {
      constexpr float SB_W    = 5.f;
      const float     thumb_h = std::max(12.f, inner * inner / content);
      const float     thumb_y = list_y + PADY + (content > inner ? scroll_y / (content - inner) * (inner - thumb_h) : 0.f);
      fill(ren, C_DIM, list_x + list_w - SB_W - 2.f, thumb_y, SB_W, thumb_h);
    }

    filter_field.render_ctx(ren, mx, my, ldown, rdown);

    // ── Resolve clicks ──────────────────────────────────────────────────────────
    if (do_can) {
      close();
      return -1;
    }
    if (chosen >= 0) {
      set_field_expr_unit(conn, schema, field_id, units[chosen].id);
      close();
      return 1;
    }
    return 0;
  }

} // namespace front
