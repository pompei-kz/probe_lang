#pragma once
#include <SDL3/SDL.h>
#include <string>

namespace front {

  struct FolderMenu
  {
    bool        open = false;
    float       x = 0, y = 0;
    int         conn_idx = -1;
    int         repo_idx = -1;
    std::string folder_id;
    std::string folder_name;

    static constexpr float W  = 210.f;
    static constexpr float IH = 26.f;
    static constexpr int   N  = 4;

    // returns 0=Добавить юнит, 1=Добавить папку, 2=Изменить, 3=Удалить, -1=none
    int render(SDL_Renderer *r, float mx, float my, bool ldown, bool rdown);
  };

} // namespace front
