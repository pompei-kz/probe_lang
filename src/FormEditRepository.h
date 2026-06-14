#pragma once
#include "Conn.h"
#include "InputField.h"
#include "SchemaNode.h"
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
    Conn           conn;

    void open_for(const Conn &c);
    void open_edit_for(const Conn &c, const RepoNode &repo);

    // returns 0=open, 1=saved, -1=cancelled
    int render(SDL_Renderer *ren, float mx, float my, bool ldown, bool rdown, int clicks);
};
