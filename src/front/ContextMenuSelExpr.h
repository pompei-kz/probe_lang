#pragma once
#include <SDL3/SDL.h>
#include <string>

namespace front {

  // Контекстное меню выбора выражения — the multi-level menu that picks what a
  // field's type expression is. Opened on the empty-expression cube square.
  // Mouse-driven and owned by EditorView (no keyboard/App wiring). It only
  // chooses *what* the expression is; EditorView performs the persistence.
  //
  // Levels (will grow):
  //   [1] "Этот" → submenu [2] "Объект" / "Юнит" / "Метод"  (the "self" kinds)
  //   [1] "Юнит" → asks EditorView to open the unit-selection form
  struct ContextMenuSelExpr
  {
    // What render() resolved to this frame (None until the user picks something).
    enum class Action { None, SetThisObject, SetThisUnit, SetThisMethod, OpenUnitForm };

    bool        open = false;
    float       x = 0, y = 0;     // screen anchor (top-left of the level-1 box)
    std::string field_id;         // the field whose expression is being chosen
    bool        submenu_open = false; // "Этот" submenu expanded
    bool        just_opened  = false; // ignore the opening click's lingering press for one frame

    void open_at(float ax, float ay, const std::string &fid);
    void close();

    // Draw + handle one frame. `ldown` is a fresh left-press this frame. Closes
    // itself (and returns an Action / None) when the user picks or clicks away.
    Action render(SDL_Renderer *ren, float mx, float my, bool ldown);
  };

} // namespace front
