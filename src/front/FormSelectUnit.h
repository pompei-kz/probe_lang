#pragma once
#include "InputField.h"
#include "back/model/ConnStore.h"
#include "back/model/Unit.h"
#include <SDL3/SDL.h>
#include <string>
#include <vector>

namespace front {

  // Форма выбора юнита — a modal list of units used to point a field's Unit
  // expression at a unit. The list is filtered (the filter field on top matches
  // units whose name contains the filter's words in order) and lazily paginated:
  // more pages are appended as the user scrolls toward the bottom, and a dim
  // "(Всё, данных больше нет)" line shows once the list is exhausted.
  //
  // Picking a unit persists unit_e.type = 'Unit' + unit_e_unit.unit_id and the
  // form returns 1; the caller then refreshes the editor.
  struct FormSelectUnit
  {
    bool              open = false;
    back::model::ConnStore conn;
    std::string       schema;
    std::string       field_id; // the field whose expression we are setting

    InputField                     filter_field;
    // Focus order: 0 = filter, 1 = Cancel (index >= 1 is a button).
    static constexpr int           FOCUS_COUNT = 2, FIRST_BUTTON = 1, CANCEL = 1;
    int                            focus    = 0;
    bool                           activate = false; // Enter pressed on the focused button
    std::vector<back::model::Unit> units;            // loaded so far (this filter)
    int                            next_offset = 0; // DB offset for the next page
    bool                           has_more    = true;
    std::string                    applied_filter;  // filter the loaded pages reflect
    float                          scroll_y = 0.f;

    void open_for(const back::model::ConnStore &c, const std::string &schema, const std::string &field_id);
    void close();

    void on_scroll(float dy);
    bool mouse_over_list(float mx, float my) const; // for wheel routing in the main loop

    // 0 = still open, 1 = a unit was chosen and persisted, -1 = cancelled.
    int render(SDL_Renderer *ren, float mx, float my, bool ldown, bool rdown, int clicks);

  private:
    void reset_for_filter(const std::string &filter); // clear + start over for a new filter
    void load_next_page();

    // List rect from the last render (used by mouse_over_list / on_scroll).
    float list_x = 0, list_y = 0, list_w = 0, list_h = 0;
  };

} // namespace front
