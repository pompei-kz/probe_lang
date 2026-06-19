//
// Created by pompei on 2026-06-19.
//

#pragma once
#include <string>

namespace back::model {

  struct Conn
  {
    std::string host, port, user, pass, dbname;

    auto operator<=>(const Conn &) const = default;

    std::string dsn()
    {
      std::string cs = "host=" + host;
      cs += " port=" + (port.empty() ? "5432" : port);
      cs += " dbname=" + (dbname.empty() ? "postgres" : dbname);
      if (!user.empty()) cs += " user=" + user;
      if (!pass.empty()) cs += " password=" + pass;
      cs += " connect_timeout=5";
      return cs;
    }
  };

} // namespace back::model
