#pragma once
#include "back/model/ConnStore.h"
#include <filesystem>
#include <string>
#include <vector>

namespace back {

  std::filesystem::path    ws_dir();
  std::vector<model::ConnStore> load_all();

} // namespace back
