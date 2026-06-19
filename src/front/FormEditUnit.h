#pragma once
#include "InputField.h"
#include "SelectableText.h"
#include "back/model/ConnStore.h"
#include "back/model/Unit.h"
#include <SDL3/SDL.h>
#include <string>

namespace front {

  struct FormEditUnit
  {
    bool              open     = false;
    bool              editing  = false;
    int               conn_idx = -1;
    int               repo_idx = -1;
    std::string       unit_id;          // set when editing
    std::string       parent_folder_id; // set when adding (empty = repo root)
    std::string       schema_name;
    back::model::ConnStore conn;

    back::model::UnitType type               = back::model::UnitType::Class;
    bool                  type_dropdown_open = false;

    InputField     name_field;
    SelectableText err_view;

    // Focus order: 0 = name, 1 = type combo, 2 = Save, 3 = Cancel (>= 2 are buttons).
    static constexpr int FOCUS_COUNT = 4, FIRST_BUTTON = 2, COMBO = 1, SAVE = 2, CANCEL = 3;
    int                  focus    = 0;
    bool                 activate = false; // Enter pressed on the focused button

    void open_add(int ci, int ri, const back::model::ConnStore &c, const std::string &schema, const std::string &parent_id);
    void open_edit(int                      ci,
                   int                      ri,
                   const back::model::ConnStore &c,
                   const std::string       &schema,
                   const std::string       &uid,
                   const std::string       &uname,
                   back::model::UnitType    utype);

    // returns 0=open, 1=saved, -1=cancelled
    int render(SDL_Renderer *ren, float mx, float my, bool ldown, bool rdown, int clicks);
  };

} // namespace front
