#pragma once
#include "back/model/ConnStore.h"
#include "back/model/FolderNode.h"
#include <pqxx/pqxx>
#include <string>
#include <utility>
#include <vector>

namespace back {

  // Load all folders for a schema into a tree (root folders returned).
  std::vector<model::FolderNode> load_folders_for_schema(pqxx::work &txn, pqxx::connection &pg, const std::string &sch);

  // Load root folders for one repo schema (rebuilds tree structure)
  std::pair<bool, std::string> load_repo_folders(const model::ConnStore &c, const std::string &schema, std::vector<model::FolderNode> &root_folders);

} // namespace back
