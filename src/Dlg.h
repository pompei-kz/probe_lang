#pragma once
#include <string>
#include "TextEditor.h"
#include "ContextMenu.h"
#include "Conn.h"

struct Dlg {
    bool        open          = false;
    bool        editing       = false;
    std::string old_name;
    TextEditor  editors[5];
    int         focus         = 0;
    std::string err;
    int         active_drag_ed = -1;
    ContextMenu ctx_menu;

    void open_add();
    void open_edit(const Conn& c);
    Conn to_conn() const;
};
