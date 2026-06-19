#include "back/service/FolderServiceR.h"
#include "back/service/RepoServiceR.h"
#include <tuple>

namespace back {

  // Build folder tree from flat list (parent_id == "" means root)
  static std::vector<model::FolderNode> build_folder_tree(const std::vector<std::tuple<std::string, std::string, std::string>> &flat,
                                                          const std::string                                                    &parent_id)
  {
    std::vector<model::FolderNode> result;
    for (const auto &[id, pid, name] : flat) {
      if (pid == parent_id) {
        model::FolderNode node{id, pid, name, build_folder_tree(flat, id)};
        result.push_back(std::move(node));
      }
    }
    return result;
  }

  // Load all folders for a schema into a flat list, then build tree
  std::vector<model::FolderNode> load_folders_for_schema(pqxx::work &txn, pqxx::connection &pg, const std::string &sch)
  {
    // Check if folder table exists
    auto check = txn.exec("SELECT 1 FROM information_schema.tables "
                          "WHERE table_schema = $1 AND table_name = 'folder' LIMIT 1",
                          pqxx::params{txn, sch});
    if (check.empty()) return {};

    auto rows = txn.exec("SELECT id, COALESCE(parent_id, ''), name "
                         "FROM " +
                         pg.quote_name(sch) + ".folder ORDER BY name");

    std::vector<std::tuple<std::string, std::string, std::string>> flat;
    for (const auto &row : rows)
      flat.emplace_back(row[0].c_str(), row[1].c_str(), row[2].c_str());
    return build_folder_tree(flat, "");
  }

  std::pair<bool, std::string> load_repo_folders(const model::ConnStore &c, const std::string &schema, std::vector<model::FolderNode> &root_folders)
  {
    root_folders.clear();
    try {
      pqxx::connection pg(make_cs(c));
      pqxx::work       txn(pg);
      root_folders = load_folders_for_schema(txn, pg, schema);
      txn.commit();
      return {true, ""};
    } catch (const std::exception &e) {
      return {false, e.what()};
    }
  }

} // namespace back
