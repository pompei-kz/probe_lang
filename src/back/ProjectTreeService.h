#pragma once
#include <filesystem>
#include <string>
#include <vector>

// Persistence of the project tree's open/closed state.
//
// Each open branch is represented by an empty marker file in
// ~/.config/probe_lang/project_tree_open_nodes/. The file name is the branch
// path — the identifiers of the parent branches plus the branch itself —
// joined with '#'. The file is created when a branch opens and removed when it
// closes.
namespace back {

  // Directory holding the open-node marker files.
  std::filesystem::path project_tree_open_nodes_dir();

  // Mark the node (identified by the path of branch ids from root to it) as
  // open: creates its empty marker file.
  void open_tree_node(const std::vector<std::string> &path);

  // Mark the node as closed: removes its marker file (no-op if absent).
  void close_tree_node(const std::vector<std::string> &path);

  // Whether the node's marker file currently exists.
  bool is_tree_node_open(const std::vector<std::string> &path);

} // namespace back
