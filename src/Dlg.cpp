#include "Dlg.h"

void Dlg::open_add()
{
  for (int i = 0; i < 5; i++) {
    editors[i]        = TextEditor{};
    editors[i].is_pwd = (i == 4);
  }
  focus          = 0;
  err            = "";
  editing        = false;
  old_name       = "";
  active_drag_ed = -1;
  ctx_menu.open  = false;
}

void Dlg::open_edit(const Conn &c)
{
  open_add();
  editors[0].set(c.name);
  editors[1].set(c.host);
  editors[2].set(c.port);
  editors[3].set(c.user);
  editors[4].set(c.pass);
  editing  = true;
  old_name = c.name;
}

Conn Dlg::to_conn() const
{
  return {editors[0].buf, editors[1].buf, editors[2].buf, editors[3].buf, editors[4].buf};
}
