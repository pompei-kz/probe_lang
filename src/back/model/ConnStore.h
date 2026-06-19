#pragma once
#include "Conn.h"

#include <string>

namespace back::model {

  struct ConnStore
  {
    std::string name, host, port, user, pass, dbname;
    bool        connected = false;

    Conn conn() const { return {.host = host, .port = port, .user = user, .pass = dbname}; }
  };

} // namespace back::model
