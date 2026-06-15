#pragma once
#include "back/Conn.h"
#include "InputField.h"
#include "back/SchemaNode.h"
#include "SelectableText.h"
#include <SDL3/SDL.h>
#include <string>

struct FormEditRepository {
    bool           open            = false;
    bool           editing         = false;
    std::string    original_schema;
    InputField     fields[2];  // [0]=schema name, [1]=repo name
    int            focus = 0;
    SelectableText err_view;
    back::Conn     conn;

    void open_for(const back::Conn &c);
    void open_edit_for(const back::Conn &c, const back::RepoNode &repo);

    // returns 0=open, 1=saved, -1=cancelled
    int render(SDL_Renderer *ren, float mx, float my, bool ldown, bool rdown, int clicks);
};
