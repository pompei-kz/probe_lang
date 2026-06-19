#pragma once
#include "back/model/Conn.h"
#include <string>

namespace back {

  void save_conn(const model::Conn &c, const std::string &old_name = "");
  void delete_conn(const std::string &name);

} // namespace back
