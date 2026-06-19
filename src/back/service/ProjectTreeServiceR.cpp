#include "back/service/ProjectTreeServiceR.h"
#include <cstdlib>
#include <fstream>

namespace back {

  namespace fs = std::filesystem;

  static const char *PROG = "probe_lang";

  fs::path project_tree_open_nodes_dir()
  {
    const char *home = std::getenv("HOME");
    return fs::path(home ? home : ".") / ".config" / PROG / "project_tree_open_nodes";
  }

  // Branch path -> "id0#id1#...#idN".
  static std::string key_of(const std::vector<std::string> &path)
  {
    std::string key;
    for (std::size_t i = 0; i < path.size(); i++) {
      if (i) key += '#';
      key += path[i];
    }
    return key;
  }

  bool is_tree_node_open(const std::vector<std::string> &path)
  {
    std::error_code ec;
    return fs::exists(project_tree_open_nodes_dir() / key_of(path), ec);
  }

} // namespace back
