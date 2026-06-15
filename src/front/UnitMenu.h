#pragma once
#include "back/model/Unit.h"
#include <SDL3/SDL.h>
#include <string>

namespace front {

  struct UnitMenu
  {
    bool                  open = false;
    float                 x = 0, y = 0;
    int                   conn_idx = -1;
    int                   repo_idx = -1;
    std::string           unit_id;
    std::string           unit_name;
    back::model::UnitType unit_type = back::model::UnitType::Class;

    static constexpr float W  = 180.f;
    static constexpr float IH = 26.f;
    static constexpr int   N  = 1;

    // returns 0=Изменить юнит, -1=none
    int render(SDL_Renderer *r, float mx, float my, bool ldown, bool rdown);
  };

} // namespace front
