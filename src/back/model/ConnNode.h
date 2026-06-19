#pragma once
#include "ConnStore.h"
#include "SchemaNode.h"
#include <string>
#include <vector>

namespace back::model {

  struct ConnNode
  {
    ConnStore                  conn;
    bool                  open = false;
    std::vector<RepoNode> repos;
  };

} // namespace back::model
