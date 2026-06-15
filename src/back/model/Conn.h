#pragma once
#include <string>

namespace back::model {

  struct Conn
  {
    std::string name, host, port, user, pass, dbname;
    bool        connected = false;
  };

} // namespace back::model
