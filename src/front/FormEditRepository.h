#pragma once
#include "InputField.h"
#include "SelectableText.h"
#include "back/model/ConnStore.h"
#include "back/model/SchemaNode.h"
#include <SDL3/SDL.h>
#include <string>

namespace front {

  struct FormEditRepository
  {
    bool              open    = false;
    bool              editing = false;
    std::string       original_schema;
    InputField        fields[2]; // [0]=schema name, [1]=repo name
    // Focus order: 0,1 = fields, 2 = Save, 3 = Cancel (indices >= 2 are buttons).
    static constexpr int FOCUS_COUNT = 4, FIRST_BUTTON = 2, SAVE = 2, CANCEL = 3;
    int               focus    = 0;
    bool              activate = false; // Enter pressed on the focused button
    SelectableText    err_view;
    back::model::ConnStore conn;

    void open_for(const back::model::ConnStore &c);
    void open_edit_for(const back::model::ConnStore &c, const back::model::RepoNode &repo);

    // returns 0=open, 1=saved, -1=cancelled
    int render(SDL_Renderer *ren, float mx, float my, bool ldown, bool rdown, int clicks);
  };

} // namespace front
