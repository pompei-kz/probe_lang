#pragma once
#include "FolderNode.h"
#include <string>
#include <vector>

namespace back::model {

  struct RepoNode
  {
    std::string             schema_name;
    std::string             repo_name;
    std::vector<FolderNode> folders; // root folders (parent_id IS NULL)
    bool                    open = false;
  };

} // namespace back::model
