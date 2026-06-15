#pragma once
#include "InputField.h"
#include "SelectableText.h"
#include "back/model/Conn.h"
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
    int               focus = 0;
    SelectableText    err_view;
    back::model::Conn conn;

    void open_for(const back::model::Conn &c);
    void open_edit_for(const back::model::Conn &c, const back::model::RepoNode &repo);

    // returns 0=open, 1=saved, -1=cancelled
    int render(SDL_Renderer *ren, float mx, float my, bool ldown, bool rdown, int clicks);
  };

} // namespace front
