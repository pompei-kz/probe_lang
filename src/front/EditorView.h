#pragma once
#include "InputField.h"
#include "back/model/Conn.h"
#include "back/model/Block.h"
#include "back/model/Unit.h"
#include <SDL3/SDL.h>
#include <string>
#include <vector>

namespace front {

  // One open unit in the editor: its own canvas (blocks scoped to this unit)
  // plus an independent camera (pan/zoom remembered per tab).
  struct EditorTab
  {
    back::model::Conn     conn;
    std::string           schema;
    std::string           unit_id;
    std::string           unit_name;
    back::model::UnitType unit_type = back::model::UnitType::Class;

    double cam_x = 0, cam_y = 0, zoom = 1.0;
    bool   cam_init = false;
    float  last_cw = -1, last_ch = -1;

    std::vector<back::model::Block> blocks;
  };

  // Tabbed graphical canvas in the right pane. A tab is opened by double-clicking
  // a unit in the tree; tabs are selected with the left button and closed with
  // the middle button. The canvas shows the active unit's blocks within the
  // visible world rectangle (spatial query), with pan (drag) and zoom (wheel).
  struct EditorView
  {
    bool                   open = false;
    std::vector<EditorTab> tabs;
    int                    active = -1;

    // Pane and canvas (pane minus the tab strip) geometry from the last render.
    float px = 0, py = 0, pw = 0, ph = 0;
    float cx = 0, cy = 0, cw = 0, ch = 0;

    // Pan drag (middle button).
    bool  panning = false, panned = false;
    float pan_last_x = 0, pan_last_y = 0;

    // Block drag (left button).
    bool        dragging = false, drag_moved = false;
    std::string drag_id;
    float       drag_last_x = 0, drag_last_y = 0;

    // Type-chooser popup (double-click on empty canvas).
    bool  chooser_open = false;
    float chooser_wx = 0, chooser_wy = 0; // world coords of the remembered click
    float chooser_sx = 0, chooser_sy = 0; // screen coords for the popup

    // Inline name editing. When edit_is_arg is set the field edits a method
    // argument (edit_arg_id); edit_id then holds the owning method's block id.
    bool                   editing = false;
    std::string            edit_id;
    back::model::BlockType edit_type = back::model::BlockType::Method;
    bool                   edit_is_arg = false;
    std::string            edit_arg_id;
    InputField             edit_field;
    float                  edit_bx = 0, edit_by = 0, edit_bw = 0, edit_bh = 0;

    // Open a unit (focus its tab if already open, else add a new one).
    void open_for(const back::model::Conn &c, const std::string &schema, const std::string &uid, const std::string &uname, back::model::UnitType utype);
    void close();          // close the whole editor
    void close_tab(int i); // close one tab

    EditorTab       *cur() { return active >= 0 && active < static_cast<int>(tabs.size()) ? &tabs[active] : nullptr; }
    const EditorTab *cur() const { return active >= 0 && active < static_cast<int>(tabs.size()) ? &tabs[active] : nullptr; }

    // World <-> screen using the active tab's camera and the canvas rect.
    double to_world_x(float sx) const { auto t = cur(); return t ? t->cam_x + (sx - cx) / t->zoom : 0; }
    double to_world_y(float sy) const { auto t = cur(); return t ? t->cam_y + (sy - cy) / t->zoom : 0; }
    float  to_screen_x(double wx) const { auto t = cur(); return t ? cx + static_cast<float>((wx - t->cam_x) * t->zoom) : cx; }
    float  to_screen_y(double wy) const { auto t = cur(); return t ? cy + static_cast<float>((wy - t->cam_y) * t->zoom) : cy; }

    // Index of the tab under the cursor, or -1.
    int tab_at(float mx, float my) const;

    void reload(); // spatial query for the active tab's current viewport

    void render(SDL_Renderer *ren, float pane_x, float pane_y, float pane_w, float pane_h, float mx, float my, bool ldown, bool rdown, int clicks);

    // Event hooks driven from the main loop.
    void on_wheel(float dy, float mx, float my);
    void on_mouse_move(float mx, float my);
    void on_mouse_up();                       // left button up: finish a block drag
    void on_middle_down(float mx, float my);  // close a tab, or begin panning the canvas
    void on_middle_up();                      // middle button up: finish panning
    bool handle_key(SDL_Keycode key, SDL_Keymod mod);
    void handle_text(const char *t);

  private:
    void init_camera();
    void save_view_state(); // persist the active tab's zoom + offset
    void start_edit_name(const back::model::Block &s, float fbx, float fby, float fbw, float fbh);
    void start_edit_arg(const back::model::Block &m, const back::model::MethodArg &a, float fbx, float fby, float fbw, float fbh);
    void add_arg(const back::model::Block &m);  // append a new argument, persist, resize
    void commit_edit();
  };

} // namespace front
