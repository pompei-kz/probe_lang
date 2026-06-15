#pragma once
#include "Conn.h"
#include "SchemaNode.h"
#include <string>
#include <vector>

namespace back {

  struct ConnNode
  {
    Conn                  conn;
    bool                  open = false;
    std::vector<RepoNode> repos;
  };

} // namespace back
