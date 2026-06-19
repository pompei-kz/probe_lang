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
  };

} // namespace back::model
