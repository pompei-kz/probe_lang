#pragma once
#include "InputField.h"
#include "back/model/Conn.h"
#include <string>

namespace front {

  struct Dlg
  {
    bool        open    = false;
    bool        editing = false;
    std::string old_name;
    InputField  fields[6]; // 0=Name 1=Host 2=Port 3=DBName 4=User 5=Password
    int         focus = 0;
    std::string err;

    bool        test_ok = false;
    std::string test_msg;
    std::string snap_host, snap_port, snap_dbname, snap_user, snap_pass;

    bool save_enabled() const
    {
      return test_ok && fields[1].ed.buf == snap_host && fields[2].ed.buf == snap_port && fields[3].ed.buf == snap_dbname &&
             fields[4].ed.buf == snap_user && fields[5].ed.buf == snap_pass;
    }

    void              open_add();
    void              open_edit(const back::model::Conn &c);
    back::model::Conn to_conn() const;
  };

} // namespace front
