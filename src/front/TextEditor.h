#pragma once
#include "DragState.h"
#include "TxAction.h"
#include "TxSnapshot.h"
#include <SDL3/SDL.h>
#include <string>
#include <utility>
#include <vector>

namespace front {

  struct TextEditor
  {
    std::string buf;
    int32_t     cursor    = 0;
    int32_t     sel_start = -1;
    float       view_px   = 0.f;
    bool        is_pwd    = false;

    std::vector<TxSnapshot> undo_stack;
    std::vector<TxSnapshot> redo_stack;
    TxAction                last_action = TxAction::None;

    DragState drag_state    = DragState::None;
    int32_t   press_bpos    = 0;
    float     press_abs_x   = 0.f;
    float     press_text_ox = 0.f;

    void set(const std::string &s);

    void push_undo(TxAction action);
    void do_undo();
    void do_redo();

    void    delete_selection();
    void    move_to(int32_t pos, bool shift);
    void    move_by(int dir, bool shift);
    int32_t word_left_pos() const;
    int32_t word_right_pos() const;

    void do_copy();
    void do_cut();
    void do_paste();

    void handle_text(const char *text);
    bool handle_key(SDL_Keycode key, SDL_Keymod mod);

    int32_t     pwd_disp_off(int32_t byte_off) const;
    int32_t     pwd_real_off(int32_t disp_off) const;
    std::string get_display() const;
    int32_t     disp_cursor() const;
    int32_t     disp_sel_start() const;

    int32_t                     pos_at_x(float text_origin_x, float abs_x) const;
    std::pair<int32_t, int32_t> word_range_at(int32_t pos) const;

    void on_mouse_press(float text_origin_x, float abs_x, int clicks, bool shift);
    void on_mouse_move(float abs_x);
    void on_mouse_release();

    void draw(SDL_Renderer *ren, float bx, float by, float bw, float bh, bool focused);
  };

} // namespace front
