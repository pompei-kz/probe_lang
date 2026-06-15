#pragma once
#include "model/Conn.h"
#include "model/FolderNode.h"
#include <pqxx/pqxx>
#include <string>
#include <utility>
#include <vector>

namespace back {

  // Load all folders for a schema into a tree (root folders returned).
  std::vector<model::FolderNode> load_folders_for_schema(pqxx::work &txn, pqxx::connection &pg, const std::string &sch);

  // Load root folders for one repo schema (rebuilds tree structure)
  std::pair<bool, std::string> load_repo_folders(const model::Conn &c, const std::string &schema, std::vector<model::FolderNode> &root_folders);

  // Create a new folder; parent_id empty = root
  std::pair<bool, std::string> create_folder(const model::Conn &c, const std::string &schema, const std::string &parent_id, const std::string &name);

  // Rename an existing folder
  std::pair<bool, std::string> rename_folder(const model::Conn &c, const std::string &schema, const std::string &id, const std::string &new_name);

  // Delete folder and all descendants recursively (no FK — uses recursive CTE)
  std::pair<bool, std::string> delete_folder_recursive(const model::Conn &c, const std::string &schema, const std::string &id);

} // namespace back
