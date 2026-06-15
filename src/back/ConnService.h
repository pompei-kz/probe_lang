#pragma once
#include "model/Conn.h"
#include <filesystem>
#include <string>
#include <vector>

namespace back {

  std::filesystem::path    ws_dir();
  std::vector<model::Conn> load_all();
  void                     save_conn(const model::Conn &c, const std::string &old_name = "");
  void                     delete_conn(const std::string &name);

} // namespace back
