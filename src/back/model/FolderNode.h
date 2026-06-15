#pragma once
#include <string>
#include <vector>

namespace back::model {

  struct FolderNode
  {
    std::string             id;
    std::string             parent_id; // empty = top-level
    std::string             name;
    std::vector<FolderNode> children;
    bool                    open = false;
  };

} // namespace back::model
