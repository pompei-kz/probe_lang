#pragma once
#include "Unit.h"
#include <string>
#include <vector>

namespace back::model {

  struct FolderNode
  {
    std::string             id;
    std::string             parent_id; // empty = top-level
    std::string             name;
    std::vector<FolderNode> children;
    std::vector<Unit>       units; // units whose parent_folder_id == this folder
    bool                    open = false;
  };

} // namespace back::model
