//
// Created by pompei on 2026-06-19.
//

#pragma once
#include "Conn.h"

#include <string>

namespace back::model {

  class ConnHash
  {
    size_t operator()(const Conn &k) const noexcept
    {
      size_t h = 0;

      auto combine = [&](const std::string &s) {
        //
        h ^= std::hash<std::string>{}(s) + 0x9e3779b9 + (h << 6) + (h >> 2);
      };

      combine(k.host);
      combine(k.port);
      combine(k.user);
      combine(k.pass);
      combine(k.dbname);

      return h;
    }
  };

} // namespace back::model
