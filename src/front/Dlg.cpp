#include "Dlg.h"

namespace front {

  void Dlg::open_add()
  {
    for (int i = 0; i < 6; i++) {
      fields[i].ed        = TextEditor{};
      fields[i].ed.is_pwd = (i == 5);
      fields[i].ctx.open  = false;
    }
    focus     = 0;
    err       = "";
    editing   = false;
    old_name  = "";
    test_ok   = false;
    test_msg  = "";
    snap_host = snap_port = snap_dbname = snap_user = snap_pass = "";
  }

  void Dlg::open_edit(const back::model::Conn &c)
  {
    open_add();
    fields[0].ed.set(c.name);
    fields[1].ed.set(c.host);
    fields[2].ed.set(c.port);
    fields[3].ed.set(c.dbname);
    fields[4].ed.set(c.user);
    fields[5].ed.set(c.pass);
    editing     = true;
    old_name    = c.name;
    test_ok     = true;
    snap_host   = c.host;
    snap_port   = c.port;
    snap_dbname = c.dbname;
    snap_user   = c.user;
    snap_pass   = c.pass;
  }

  back::model::Conn Dlg::to_conn() const
  {
    back::model::Conn c;
    c.name   = fields[0].ed.buf;
    c.host   = fields[1].ed.buf;
    c.port   = fields[2].ed.buf;
    c.dbname = fields[3].ed.buf;
    c.user   = fields[4].ed.buf;
    c.pass   = fields[5].ed.buf;
    return c;
  }

} // namespace front
