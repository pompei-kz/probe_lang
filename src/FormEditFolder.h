#pragma once
#include "back/Conn.h"
#include "back/FolderNode.h"
#include "InputField.h"
#include "SelectableText.h"
#include <SDL3/SDL.h>
#include <string>

struct FormEditFolder {
    bool        open             = false;
    bool        editing          = false;
    int         conn_idx         = -1;
    int         repo_idx         = -1;
    std::string folder_id;        // set when editing
    std::string parent_folder_id; // set when adding (empty = root)
    std::string schema_name;
    back::Conn  conn;

    InputField     name_field;
    SelectableText err_view;

    void open_add(int ci, int ri, const back::Conn &c, const std::string &schema, const std::string &parent_id);
    void open_edit(int ci, int ri, const back::Conn &c, const std::string &schema,
                   const std::string &fid, const std::string &fname);

    // returns 0=open, 1=saved, -1=cancelled
    int render(SDL_Renderer *ren, float mx, float my, bool ldown, bool rdown, int clicks);
};
