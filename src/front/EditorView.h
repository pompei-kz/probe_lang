#pragma once
#include "InputField.h"
#include "back/model/Conn.h"
#include "back/model/Statement.h"
#include <SDL3/SDL.h>
#include <string>
#include <vector>

namespace front {

  // Graphical canvas shown in the right pane. Opened by double-clicking a unit
  // in the tree. Displays every statement (unit_st row) whose geometry falls in
  // the visible world rectangle (spatial query via the GIST index). Supports
  // pan (drag) and zoom (wheel), adding statements (double-click empty space),
  // and inline renaming (double-click a statement's name).
  struct EditorView
  {
    bool open = false;

    back::model::Conn conn;
    std::string       schema;
    std::string       unit_id;   // opened unit; used as unit_id for new statements
    std::string       unit_name; // shown as a hint in the corner

    // Camera: world point at the top-left of the pane, plus zoom (px per world unit).
    double cam_x = 0, cam_y = 0;
    double zoom  = 1.0;

    std::vector<back::model::Statement> stmts;

    // Pane rectangle as set on the most recent render (for world<->screen math).
    float px = 0, py = 0, pw = 0, ph = 0;
    bool  cam_init = false;
    float last_pw = -1, last_ph = -1;

    // Pan drag.
    bool  panning = false;
    bool  panned  = false;
    float pan_last_x = 0, pan_last_y = 0;

    // Type-chooser popup (after double-clicking empty space).
    bool  chooser_open = false;
    float chooser_wx = 0, chooser_wy = 0; // world coords of the remembered click
    float chooser_sx = 0, chooser_sy = 0; // screen coords where the popup is drawn

    // Inline name editing.
    bool                       editing = false;
    std::string                edit_id;
    back::model::StatementType edit_type = back::model::StatementType::Method;
    InputField                 edit_field;
    float                      edit_bx = 0, edit_by = 0, edit_bw = 0, edit_bh = 0;

    void open_for(const back::model::Conn &c, const std::string &schema_, const std::string &uid, const std::string &uname);
    void close();

    // World <-> screen using the pane geometry from the last render.
    double to_world_x(float sx) const { return cam_x + (sx - px) / zoom; }
    double to_world_y(float sy) const { return cam_y + (sy - py) / zoom; }
    float  to_screen_x(double wx) const { return px + static_cast<float>((wx - cam_x) * zoom); }
    float  to_screen_y(double wy) const { return py + static_cast<float>((wy - cam_y) * zoom); }

    void reload(); // spatial query for the current viewport

    // Draw the canvas and handle click/double-click/right-click interaction.
    void render(SDL_Renderer *ren, float pane_x, float pane_y, float pane_w, float pane_h, float mx, float my, bool ldown, bool rdown, int clicks);

    // Event hooks driven from the main loop.
    void on_wheel(float dy, float mx, float my); // zoom around the cursor
    void on_mouse_move(float mx, float my);       // pan / text-selection drag
    void on_mouse_up();
    bool handle_key(SDL_Keycode key, SDL_Keymod mod); // returns true if consumed
    void handle_text(const char *t);

  private:
    void init_camera();
    void start_edit(const back::model::Statement &s, float fbx, float fby, float fbw, float fbh);
    void commit_edit();
  };

} // namespace front
