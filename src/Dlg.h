#pragma once
#include "Conn.h"
#include "ContextMenu.h"
#include "TextEditor.h"
#include <string>

struct Dlg
{
  bool        open    = false;
  bool        editing = false;
  std::string old_name;
  TextEditor  editors[5];
  int         focus          = 0;
  std::string err;
  int         active_drag_ed = -1;
  ContextMenu ctx_menu;

  bool        test_ok  = false;
  std::string test_msg;
  // snapshot of host/port/user/pass at the moment the last successful test ran
  std::string snap_host, snap_port, snap_user, snap_pass;

  bool save_enabled() const {
    return test_ok
        && editors[1].buf == snap_host
        && editors[2].buf == snap_port
        && editors[3].buf == snap_user
        && editors[4].buf == snap_pass;
  }

  void open_add();
  void open_edit(const Conn &c);
  Conn to_conn() const;
};
