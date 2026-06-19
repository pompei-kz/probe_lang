#pragma once
#include <string>
#include <vector>

namespace back {

  // Mark the node (identified by the path of branch ids from root to it) as
  // open: creates its empty marker file.
  void open_tree_node(const std::vector<std::string> &path);

  // Mark the node as closed: removes its marker file (no-op if absent).
  void close_tree_node(const std::vector<std::string> &path);

} // namespace back
