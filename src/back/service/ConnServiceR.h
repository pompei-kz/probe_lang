#pragma once
#include "back/model/Conn.h"
#include <filesystem>
#include <string>
#include <vector>

namespace back {

  std::filesystem::path    ws_dir();
  std::vector<model::Conn> load_all();

} // namespace back
