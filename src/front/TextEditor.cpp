#include "TextEditor.h"
#include "FontAtlas.h"
#include "render_helpers.h"

#include <algorithm>
#include <cstring>
#include <unicode/uchar.h>
#include <unicode/utf8.h>

namespace front {

  void TextEditor::set(const std::string &s)
  {
    buf       = s;
    cursor    = (int32_t)s.size();
    sel_start = -1;
    view_px   = 0.f;
    undo_stack.clear();
    redo_stack.clear();
    last_action = TxAction::None;
    drag_state  = DragState::None;
  }

  // ── undo ──────────────────────────────────────────────────────────────────────
  void TextEditor::push_undo(TxAction action)
  {
    bool coalesce = (action == last_action) && (action == TxAction::Insert || action == TxAction::Delete) && sel_start < 0;
    if (!coalesce) {
      undo_stack.push_back({buf, cursor, sel_start});
      redo_stack.clear();
    }
    last_action = action;
  }

  void TextEditor::do_undo()
  {
    if (undo_stack.empty()) return;
    redo_stack.push_back({buf, cursor, sel_start});
    auto s = undo_stack.back();
    undo_stack.pop_back();
    buf         = s.buf;
    cursor      = s.cursor;
    sel_start   = s.sel_start;
    last_action = TxAction::None;
  }

  void TextEditor::do_redo()
  {
    if (redo_stack.empty()) return;
    undo_stack.push_back({buf, cursor, sel_start});
    auto s = redo_stack.back();
    redo_stack.pop_back();
    buf         = s.buf;
    cursor      = s.cursor;
    sel_start   = s.sel_start;
    last_action = TxAction::None;
  }

  // ── movement ──────────────────────────────────────────────────────────────────
  void TextEditor::delete_selection()
  {
    if (sel_start < 0) return;
    int32_t lo = std::min(sel_start, cursor);
    int32_t hi = std::max(sel_start, cursor);
    buf.erase((size_t)lo, (size_t)(hi - lo));
    cursor    = lo;
    sel_start = -1;
  }

  void TextEditor::move_to(int32_t pos, bool shift)
  {
    if (shift) {
      if (sel_start < 0) sel_start = cursor;
    } else
      sel_start = -1;
    cursor = std::clamp(pos, 0, (int32_t)buf.size());
  }

  void TextEditor::move_by(int dir, bool shift)
  {
    if (!shift && sel_start >= 0) {
      cursor    = dir < 0 ? std::min(sel_start, cursor) : std::max(sel_start, cursor);
      sel_start = -1;
      return;
    }
    int32_t np = cursor;
    if (dir < 0 && cursor > 0) {
      UChar32 cp;
      U8_PREV(reinterpret_cast<const uint8_t *>(buf.data()), 0, np, cp);
      (void)cp;
    } else if (dir > 0 && cursor < (int32_t)buf.size()) {
      UChar32 cp;
      U8_NEXT(reinterpret_cast<const uint8_t *>(buf.data()), np, (int32_t)buf.size(), cp);
      (void)cp;
    }
    move_to(np, shift);
  }

  int32_t TextEditor::word_left_pos() const
  {
    int32_t        pos = cursor;
    const uint8_t *s   = reinterpret_cast<const uint8_t *>(buf.data());
    while (pos > 0) {
      int32_t p = pos;
      UChar32 cp;
      U8_PREV(s, 0, p, cp);
      if (u_isalnum(cp)) break;
      pos = p;
    }
    while (pos > 0) {
      int32_t p = pos;
      UChar32 cp;
      U8_PREV(s, 0, p, cp);
      if (!u_isalnum(cp)) break;
      pos = p;
    }
    return pos;
  }

  int32_t TextEditor::word_right_pos() const
  {
    int32_t        pos = cursor, len = (int32_t)buf.size();
    const uint8_t *s = reinterpret_cast<const uint8_t *>(buf.data());
    while (pos < len) {
      int32_t p = pos;
      UChar32 cp;
      U8_NEXT(s, p, len, cp);
      if (u_isalnum(cp)) break;
      pos = p;
    }
    while (pos < len) {
      int32_t p = pos;
      UChar32 cp;
      U8_NEXT(s, p, len, cp);
      if (!u_isalnum(cp)) break;
      pos = p;
    }
    return pos;
  }

  // ── clipboard ─────────────────────────────────────────────────────────────────
  void TextEditor::do_copy()
  {
    if (sel_start < 0) return;
    int32_t lo = std::min(sel_start, cursor);
    int32_t hi = std::max(sel_start, cursor);
    SDL_SetClipboardText(buf.substr((size_t)lo, (size_t)(hi - lo)).c_str());
  }

  void TextEditor::do_cut()
  {
    if (sel_start < 0) return;
    do_copy();
    push_undo(TxAction::Other);
    delete_selection();
  }

  void TextEditor::do_paste()
  {
    char *clip = SDL_GetClipboardText();
    if (clip && *clip) {
      push_undo(TxAction::Other);
      delete_selection();
      int32_t tlen = (int32_t)strlen(clip);
      buf.insert((size_t)cursor, clip, (size_t)tlen);
      cursor += tlen;
      sel_start = -1;
    }
    SDL_free(clip);
  }

  // ── input ─────────────────────────────────────────────────────────────────────
  void TextEditor::handle_text(const char *text)
  {
    int32_t tlen = (int32_t)strlen(text);
    push_undo(tlen == 1 && sel_start < 0 ? TxAction::Insert : TxAction::Other);
    delete_selection();
    buf.insert((size_t)cursor, text, (size_t)tlen);
    cursor += tlen;
    sel_start = -1;
  }

  bool TextEditor::handle_key(SDL_Keycode key, SDL_Keymod mod)
  {
    bool ctrl  = (mod & SDL_KMOD_CTRL) != 0;
    bool shift = (mod & SDL_KMOD_SHIFT) != 0;
    switch (key) {
      case SDLK_LEFT:
        if (ctrl)
          move_to(word_left_pos(), shift);
        else
          move_by(-1, shift);
        return true;
      case SDLK_RIGHT:
        if (ctrl)
          move_to(word_right_pos(), shift);
        else
          move_by(+1, shift);
        return true;
      case SDLK_HOME: move_to(0, shift); return true;
      case SDLK_END: move_to((int32_t)buf.size(), shift); return true;
      case SDLK_BACKSPACE:
        if (sel_start >= 0) {
          push_undo(TxAction::Other);
          delete_selection();
        } else if (ctrl) {
          push_undo(TxAction::Other);
          int32_t t = word_left_pos();
          buf.erase((size_t)t, (size_t)(cursor - t));
          cursor = t;
        } else if (cursor > 0) {
          push_undo(TxAction::Delete);
          int32_t p = cursor;
          UChar32 cp;
          U8_PREV(reinterpret_cast<const uint8_t *>(buf.data()), 0, p, cp);
          (void)cp;
          buf.erase((size_t)p, (size_t)(cursor - p));
          cursor = p;
        }
        return true;
      case SDLK_DELETE:
        if (ctrl) {
          do_cut();
        } else if (sel_start >= 0) {
          push_undo(TxAction::Other);
          delete_selection();
        } else if (cursor < (int32_t)buf.size()) {
          push_undo(TxAction::Delete);
          int32_t n = cursor;
          UChar32 cp;
          U8_NEXT(reinterpret_cast<const uint8_t *>(buf.data()), n, (int32_t)buf.size(), cp);
          (void)cp;
          buf.erase((size_t)cursor, (size_t)(n - cursor));
        }
        return true;
      case SDLK_A:
        if (ctrl) {
          sel_start = 0;
          cursor    = (int32_t)buf.size();
          return true;
        }
        return false;
      case SDLK_C:
        if (ctrl) {
          do_copy();
          return true;
        }
        return false;
      case SDLK_X:
        if (ctrl) {
          do_cut();
          return true;
        }
        return false;
      case SDLK_V:
        if (ctrl) {
          do_paste();
          return true;
        }
        return false;
      case SDLK_INSERT:
        if (ctrl) {
          do_copy();
          return true;
        }
        if (shift) {
          do_paste();
          return true;
        }
        return false;
      case SDLK_Z:
        if (ctrl && shift) {
          do_redo();
          return true;
        }
        if (ctrl) {
          do_undo();
          return true;
        }
        return false;
      case SDLK_Y:
        if (ctrl) {
          do_redo();
          return true;
        }
        return false;
      default: return false;
    }
  }

  // ── password helpers ──────────────────────────────────────────────────────────
  int32_t TextEditor::pwd_disp_off(int32_t byte_off) const
  {
    int32_t        count = 0, i = 0, len = (int32_t)buf.size();
    const uint8_t *s = reinterpret_cast<const uint8_t *>(buf.data());
    while (i < byte_off && i < len) {
      UChar32 cp;
      U8_NEXT(s, i, len, cp);
      if (cp >= 0) count++;
    }
    return count;
  }

  int32_t TextEditor::pwd_real_off(int32_t disp_off) const
  {
    int32_t        count = 0, i = 0, len = (int32_t)buf.size();
    const uint8_t *s = reinterpret_cast<const uint8_t *>(buf.data());
    while (i < len && count < disp_off) {
      UChar32 cp;
      U8_NEXT(s, i, len, cp);
      if (cp >= 0) count++;
    }
    return i;
  }

  std::string TextEditor::get_display() const
  {
    if (!is_pwd) return buf;
    int32_t        count = 0, i = 0, len = (int32_t)buf.size();
    const uint8_t *s = reinterpret_cast<const uint8_t *>(buf.data());
    while (i < len) {
      UChar32 cp;
      U8_NEXT(s, i, len, cp);
      if (cp >= 0) count++;
    }
    return std::string((size_t)count, '*');
  }

  int32_t TextEditor::disp_cursor() const
  {
    return is_pwd ? pwd_disp_off(cursor) : cursor;
  }

  int32_t TextEditor::disp_sel_start() const
  {
    return sel_start < 0 ? -1 : (is_pwd ? pwd_disp_off(sel_start) : sel_start);
  }

  // ── mouse ─────────────────────────────────────────────────────────────────────
  int32_t TextEditor::pos_at_x(float text_origin_x, float abs_x) const
  {
    float       offset = abs_x - text_origin_x + view_px;
    std::string disp   = get_display();
    const char *ds     = disp.c_str();
    int32_t     dlen = (int32_t)disp.size(), i = 0, found = dlen;
    float       cur_x = 0.f;
    while (i < dlen) {
      int32_t prev_i = i;
      UChar32 cp;
      U8_NEXT(reinterpret_cast<const uint8_t *>(ds), i, dlen, cp);
      if (cp < 0) continue;
      float adv = g_atlas.get(cp).adv;
      if (offset <= cur_x + adv * 0.5f) {
        found = prev_i;
        break;
      }
      cur_x += adv;
    }
    return is_pwd ? pwd_real_off(found) : found;
  }

  std::pair<int32_t, int32_t> TextEditor::word_range_at(int32_t pos) const
  {
    int32_t len = (int32_t)buf.size();
    if (len == 0) return {0, 0};
    const uint8_t *s = reinterpret_cast<const uint8_t *>(buf.data());
    UChar32        ref;
    if (pos < len) {
      int32_t p = pos;
      U8_NEXT(s, p, len, ref);
    } else {
      int32_t p = pos;
      U8_PREV(s, 0, p, ref);
    }
    bool    alnum = u_isalnum(ref) != 0;
    int32_t lo = pos, hi = pos;
    while (lo > 0) {
      int32_t p = lo;
      UChar32 cp;
      U8_PREV(s, 0, p, cp);
      if ((u_isalnum(cp) != 0) != alnum) break;
      lo = p;
    }
    while (hi < len) {
      int32_t p = hi;
      UChar32 cp;
      U8_NEXT(s, p, len, cp);
      if ((u_isalnum(cp) != 0) != alnum) break;
      hi = p;
    }
    return {lo, hi};
  }

  void TextEditor::on_mouse_press(float text_origin_x, float abs_x, int clicks, bool shift)
  {
    press_abs_x   = abs_x;
    press_text_ox = text_origin_x;

    if (shift && clicks == 1) {
      move_to(pos_at_x(text_origin_x, abs_x), true);
      drag_state = DragState::Active;
      return;
    }
    if (clicks >= 3) {
      sel_start  = 0;
      cursor     = (int32_t)buf.size();
      drag_state = DragState::MultiClick;
      return;
    }
    if (clicks == 2) {
      auto [lo, hi] = word_range_at(pos_at_x(text_origin_x, abs_x));
      sel_start     = lo;
      cursor        = hi;
      drag_state    = DragState::MultiClick;
      return;
    }
    press_bpos = pos_at_x(text_origin_x, abs_x);
    drag_state = DragState::Pending;
  }

  void TextEditor::on_mouse_move(float abs_x)
  {
    if (drag_state == DragState::None) return;
    if (drag_state == DragState::Pending) {
      if (std::abs(abs_x - press_abs_x) < g_atlas.get('W').adv * 0.5f) return;
      drag_state = DragState::Active;
      cursor     = press_bpos;
      sel_start  = press_bpos;
    }
    cursor = pos_at_x(press_text_ox, abs_x);
  }

  void TextEditor::on_mouse_release()
  {
    if (drag_state == DragState::Pending) {
      cursor    = press_bpos;
      sel_start = -1;
    }
    drag_state = DragState::None;
  }

  // ── draw ─────────────────────────────────────────────────────────────────────
  void TextEditor::draw(SDL_Renderer *ren, float bx, float by, float bw, float bh, bool focused)
  {
    fill(ren, C_INBG, bx, by, bw, bh);
    rect(ren, focused ? C_ACCENT : C_BORDER, bx, by, bw, bh);

    constexpr float PADX    = 6.f;
    float           inner_w = bw - 2.f * PADX;
    float           text_y  = center_baseline(by, bh);

    std::string disp = get_display();
    const char *ds   = disp.c_str();
    int32_t     dlen = (int32_t)disp.size();
    int32_t     dc   = disp_cursor();
    int32_t     dss  = disp_sel_start();

    float total_w   = text_w_n(ds, dlen);
    float cursor_px = text_w_n(ds, dc);

    if (focused) {
      if (cursor_px - view_px < 0.f)
        view_px = cursor_px;
      else if (cursor_px - view_px > inner_w - 2.f)
        view_px = cursor_px - inner_w + 2.f;
    }
    view_px = std::clamp(view_px, 0.f, std::max(0.f, total_w - inner_w));

    float ox = bx + PADX - view_px;

    SDL_Rect clip{(int)(bx + 1), (int)(by + 1), (int)(bw - 2), (int)(bh - 2)};
    SDL_SetRenderClipRect(ren, &clip);

    if (focused && dss >= 0) {
      int32_t lo = std::min(dss, dc), hi = std::max(dss, dc);
      float   sx = ox + text_w_n(ds, lo);
      float   sw = text_w_n(ds + lo, hi - lo);
      SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
      fill(ren, Clr{0x89, 0xb4, 0xfa, 0x55}, sx, by + 2.f, sw, bh - 4.f);
      SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_NONE);
    }

    if (!disp.empty()) text_draw(ren, ds, ox, text_y, C_TEXT);

    if (focused && (SDL_GetTicks() / 530) % 2 == 0) fill(ren, C_TEXT, ox + cursor_px - 0.5f, by + 3.f, 1.5f, bh - 6.f);

    SDL_SetRenderClipRect(ren, nullptr);
  }

} // namespace front
