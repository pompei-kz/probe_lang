#include "SelectableText.h"
#include "FontAtlas.h"
#include "render_helpers.h"

#include <algorithm>
#include <unicode/uchar.h>
#include <unicode/utf8.h>

static constexpr float LINE_H = FS + 6.f;
static constexpr float PADX   = 6.f;
static constexpr float PADY   = 4.f;

void SelectableText::set(const std::string &s)
{
  text      = s;
  cursor    = (int32_t)s.size();
  sel_start = -1;
  scroll_y  = 0.f;
  focused   = false;
  dragging  = false;
  ctx_open  = false;
  build_lines();
}

void SelectableText::build_lines()
{
  lines.clear();
  int32_t i = 0, n = (int32_t)text.size(), ls = 0;
  while (i <= n) {
    if (i == n || text[i] == '\n') {
      lines.push_back({ls, i - ls});
      ls = i + 1;
    }
    i++;
  }
  if (lines.empty()) lines.push_back({0, 0});
}

float SelectableText::total_h() const
{
  return (float)lines.size() * LINE_H;
}

bool SelectableText::mouse_over(float mx, float my) const
{
  return box_w > 0 && hit(mx, my, box_x, box_y, box_w, box_h);
}

int32_t SelectableText::pos_at(float abs_x, int li) const
{
  if (li < 0) return 0;
  if (li >= (int)lines.size()) return (int32_t)text.size();
  const Line &ln     = lines[li];
  float       offset = abs_x - (box_x + PADX);
  if (offset <= 0) return ln.start;
  const char *s   = text.c_str() + ln.start;
  int32_t     len = ln.len, i = 0;
  float       cur_x = 0.f;
  while (i < len) {
    int32_t prev = i;
    UChar32 cp;
    U8_NEXT(reinterpret_cast<const uint8_t *>(s), i, len, cp);
    if (cp < 0) continue;
    float adv = g_atlas.get(cp).adv;
    if (offset <= cur_x + adv * 0.5f) return ln.start + prev;
    cur_x += adv;
  }
  return ln.start + ln.len;
}

int SelectableText::line_idx_at(float abs_y) const
{
  int li = (int)((abs_y - (box_y + PADY) + scroll_y) / LINE_H);
  return std::clamp(li, 0, (int)lines.size() - 1);
}

int32_t SelectableText::pos_at_mouse(float abs_x, float abs_y) const
{
  return pos_at(abs_x, line_idx_at(abs_y));
}

std::pair<int32_t,int32_t> SelectableText::word_range_at(int32_t pos) const
{
  int32_t        n = (int32_t)text.size();
  if (n == 0) return {0, 0};
  const uint8_t *s = reinterpret_cast<const uint8_t *>(text.data());

  UChar32 ref = ' ';
  if (pos < n) {
    int32_t p = pos;
    U8_NEXT(s, p, n, ref);
  } else if (pos > 0) {
    int32_t p = pos;
    U8_PREV(s, 0, p, ref);
  }
  bool alnum = u_isalnum(ref) != 0;

  int32_t lo = pos, hi = pos;
  while (lo > 0) {
    int32_t p = lo;
    UChar32 cp;
    U8_PREV(s, 0, p, cp);
    if ((u_isalnum(cp) != 0) != alnum) break;
    lo = p;
  }
  while (hi < n) {
    int32_t p = hi;
    UChar32 cp;
    U8_NEXT(s, p, n, cp);
    if ((u_isalnum(cp) != 0) != alnum) break;
    hi = p;
  }
  return {lo, hi};
}

void SelectableText::do_copy() const
{
  if (text.empty()) return;
  if (sel_start < 0 || sel_start == cursor) {
    SDL_SetClipboardText(text.c_str());
  } else {
    int32_t lo = std::min(sel_start, cursor);
    int32_t hi = std::max(sel_start, cursor);
    SDL_SetClipboardText(text.substr((size_t)lo, (size_t)(hi - lo)).c_str());
  }
}

bool SelectableText::handle_key(SDL_Keycode key, SDL_Keymod mod)
{
  bool ctrl  = (mod & SDL_KMOD_CTRL) != 0;
  bool shift = (mod & SDL_KMOD_SHIFT) != 0;
  switch (key) {
    case SDLK_A:
      if (ctrl) { sel_start = 0; cursor = (int32_t)text.size(); return true; }
      return false;
    case SDLK_C:
      if (ctrl) { do_copy(); return true; }
      return false;
    case SDLK_INSERT:
      if (ctrl) { do_copy(); return true; }
      return false;
    case SDLK_HOME:
      if (shift && sel_start < 0) sel_start = cursor;
      else if (!shift) sel_start = -1;
      cursor   = 0;
      scroll_y = 0.f;
      return true;
    case SDLK_END:
      if (shift && sel_start < 0) sel_start = cursor;
      else if (!shift) sel_start = -1;
      cursor   = (int32_t)text.size();
      scroll_y = std::max(0.f, total_h() - (box_h - 2.f * PADY));
      return true;
    default: return false;
  }
}

void SelectableText::on_move(float mx, float my)
{
  if (!dragging) return;
  cursor = pos_at_mouse(mx, my);
}

void SelectableText::on_release()
{
  dragging = false;
}

void SelectableText::on_scroll(float dy)
{
  float inner_h    = std::max(1.f, box_h - 2.f * PADY);
  float max_scroll = std::max(0.f, total_h() - inner_h);
  scroll_y         = std::clamp(scroll_y - dy * LINE_H * 3.f, 0.f, max_scroll);
}

void SelectableText::render(SDL_Renderer *ren, float bx, float by, float bw, float bh,
                            float mx, float my, bool ldown, bool rdown,
                            Clr text_clr, int clicks)
{
  box_x = bx; box_y = by; box_w = bw; box_h = bh;

  if (lines.empty()) build_lines();

  float inner_h    = bh - 2.f * PADY;
  float content_h  = total_h();
  float max_scroll = std::max(0.f, content_h - inner_h);
  scroll_y         = std::clamp(scroll_y, 0.f, max_scroll);

  fill(ren, C_INBG, bx, by, bw, bh);
  rect(ren, focused ? C_ACCENT : C_BORDER, bx, by, bw, bh);

  SDL_Rect clip{(int)(bx + 1), (int)(by + 1), (int)(bw - 2), (int)(bh - 2)};
  SDL_SetRenderClipRect(ren, &clip);

  float text_x = bx + PADX;

  for (int li = 0; li < (int)lines.size(); li++) {
    const Line &ln     = lines[li];
    float       line_y = by + PADY + li * LINE_H - scroll_y;

    if (line_y + LINE_H < by || line_y > by + bh) continue;

    // selection highlight
    if (sel_start >= 0 && sel_start != cursor) {
      int32_t lo       = std::min(sel_start, cursor);
      int32_t hi       = std::max(sel_start, cursor);
      int32_t line_end = ln.start + ln.len;

      if (lo <= line_end && hi > ln.start) {
        int32_t    slo = std::max(lo, ln.start) - ln.start;
        int32_t    shi = std::min(hi, line_end) - ln.start;
        const char *ls = text.c_str() + ln.start;
        float hx = text_x + text_w_n(ls, slo);
        float hw = text_w_n(ls + slo, shi - slo);
        if (hi > line_end && li + 1 < (int)lines.size())
          hw += g_atlas.get(' ').adv * 0.6f;
        SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
        fill(ren, Clr{0x89, 0xb4, 0xfa, 0x55}, hx, line_y, hw, LINE_H);
        SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_NONE);
      }
    }

    if (ln.len > 0)
      text_draw_n(ren, text.c_str() + ln.start, ln.len, text_x,
                  center_baseline(line_y, LINE_H), text_clr);
  }

  // scrollbar
  if (content_h > inner_h + 0.5f) {
    constexpr float SB_W = 5.f;
    float thumb_h = std::max(12.f, inner_h * inner_h / content_h);
    float thumb_y = by + PADY + scroll_y / content_h * inner_h;
    fill(ren, C_DIM, bx + bw - SB_W - 2.f, thumb_y, SB_W, thumb_h);
  }

  SDL_SetRenderClipRect(ren, nullptr);

  bool over_box = hit(mx, my, bx, by, bw, bh);

  // context menu
  constexpr float CMW = 130.f, CMIH = 26.f;
  if (ctx_open) {
    bool over_ctx = hit(mx, my, ctx_x, ctx_y, CMW, CMIH + 4.f);
    if ((ldown || rdown) && !over_ctx) {
      ctx_open = false;
    } else {
      fill(ren, C_DLGBG, ctx_x, ctx_y, CMW, CMIH + 4.f);
      rect(ren, C_BORDER, ctx_x, ctx_y, CMW, CMIH + 4.f);
      float iy  = ctx_y + 2.f;
      bool  hov = hit(mx, my, ctx_x + 1.f, iy, CMW - 2.f, CMIH);
      if (hov) fill(ren, C_HOVER, ctx_x + 1.f, iy, CMW - 2.f, CMIH);
      text_draw(ren, "Скопировать", ctx_x + 12.f, center_baseline(iy, CMIH), C_TEXT);
      if (ldown && hov) {
        do_copy();
        ctx_open = false;
      }
    }
  }

  if (rdown && over_box) {
    focused  = true;
    ctx_x    = std::min(mx, bx + bw - CMW - 2.f);
    ctx_y    = std::min(my, by + bh - CMIH - 6.f);
    ctx_open = true;
  }

  if (ldown) {
    if (over_box && !ctx_open) {
      focused = true;
      if (clicks >= 4 || (clicks >= 3 && lines.size() == 1)) {
        // 4+ clicks (or 3+ on single line): select all
        sel_start = 0;
        cursor    = (int32_t)text.size();
        dragging  = false;
      } else if (clicks == 3) {
        // triple click: select whole line
        int li    = line_idx_at(my);
        sel_start = lines[li].start;
        cursor    = lines[li].start + lines[li].len;
        dragging  = false;
      } else if (clicks == 2) {
        // double click: select word
        auto [lo, hi] = word_range_at(pos_at_mouse(mx, my));
        sel_start     = lo;
        cursor        = hi;
        dragging      = false;
      } else {
        // single click
        int32_t p  = pos_at_mouse(mx, my);
        bool    sh = (SDL_GetModState() & SDL_KMOD_SHIFT) != 0;
        if (sh) {
          if (sel_start < 0) sel_start = cursor;
        } else {
          sel_start = p;
        }
        cursor   = p;
        dragging = true;
      }
    } else if (!over_box && !ctx_open) {
      focused  = false;
      dragging = false;
    }
  }
}
