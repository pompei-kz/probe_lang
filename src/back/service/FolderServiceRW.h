#pragma once
#include "back/model/ConnStore.h"
#include <string>
#include <utility>

namespace back {

  // Create a new folder; parent_id empty = root
  std::pair<bool, std::string> create_folder(const model::ConnStore &c, const std::string &schema, const std::string &parent_id, const std::string &name);

  // Rename an existing folder
  std::pair<bool, std::string> rename_folder(const model::ConnStore &c, const std::string &schema, const std::string &id, const std::string &new_name);

  // Delete folder and all descendants recursively (no FK — uses recursive CTE)
  std::pair<bool, std::string> delete_folder_recursive(const model::ConnStore &c, const std::string &schema, const std::string &id);

} // namespace back
