#include "Dlg.h"

void Dlg::open_add()
{
  for (int i = 0; i < 6; i++) {
    editors[i]        = TextEditor{};
    editors[i].is_pwd = (i == 5);
  }
  focus          = 0;
  err            = "";
  editing        = false;
  old_name       = "";
  active_drag_ed = -1;
  ctx_menu.open  = false;
  test_ok        = false;
  test_msg       = "";
  snap_host = snap_port = snap_dbname = snap_user = snap_pass = "";
}

void Dlg::open_edit(const Conn &c)
{
  open_add();
  editors[0].set(c.name);
  editors[1].set(c.host);
  editors[2].set(c.port);
  editors[3].set(c.dbname);
  editors[4].set(c.user);
  editors[5].set(c.pass);
  editing      = true;
  old_name     = c.name;
  test_ok      = true;
  snap_host    = c.host;
  snap_port    = c.port;
  snap_dbname  = c.dbname;
  snap_user    = c.user;
  snap_pass    = c.pass;
}

Conn Dlg::to_conn() const
{
  Conn c;
  c.name   = editors[0].buf;
  c.host   = editors[1].buf;
  c.port   = editors[2].buf;
  c.dbname = editors[3].buf;
  c.user   = editors[4].buf;
  c.pass   = editors[5].buf;
  return c;
}
