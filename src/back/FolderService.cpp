#include "FolderService.h"
#include "CustomId.h"
#include "DbInternal.h"
#include "InitDb.h"
#include <tuple>

namespace back {

  using namespace model;

  // Build folder tree from flat list (parent_id == "" means root)
  static std::vector<FolderNode> build_folder_tree(const std::vector<std::tuple<std::string, std::string, std::string>> &flat,
                                                   const std::string                                                    &parent_id)
  {
    std::vector<FolderNode> result;
    for (const auto &[id, pid, name] : flat) {
      if (pid == parent_id) {
        FolderNode node{id, pid, name, build_folder_tree(flat, id)};
        result.push_back(std::move(node));
      }
    }
    return result;
  }

  // Load all folders for a schema into a flat list, then build tree
  std::vector<FolderNode> load_folders_for_schema(pqxx::work &txn, pqxx::connection &pg, const std::string &sch)
  {
    // Check if folder table exists
    auto check = txn.exec_params("SELECT 1 FROM information_schema.tables "
                                 "WHERE table_schema = $1 AND table_name = 'folder' LIMIT 1",
                                 sch);
    if (check.empty()) return {};

    auto rows = txn.exec("SELECT id, COALESCE(parent_id, ''), name "
                         "FROM " +
                         pg.quote_name(sch) + ".folder ORDER BY name");

    std::vector<std::tuple<std::string, std::string, std::string>> flat;
    for (const auto &row : rows)
      flat.emplace_back(row[0].c_str(), row[1].c_str(), row[2].c_str());
    return build_folder_tree(flat, "");
  }

  // ── Public API ────────────────────────────────────────────────────────────────

  std::pair<bool, std::string> load_repo_folders(const Conn &c, const std::string &schema, std::vector<FolderNode> &root_folders)
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

  std::pair<bool, std::string> create_folder(const Conn &c, const std::string &schema, const std::string &parent_id, const std::string &name)
  {
    try {
      pqxx::connection pg(make_cs(c));
      pqxx::work       txn(pg);
      std::string      qsch = pg.quote_name(schema);

      // Create table in case repo was added before folder feature was introduced
      init_folder_table(txn, pg, schema);

      std::string id = new_id();
      if (parent_id.empty()) {
        txn.exec_params("INSERT INTO " + qsch + ".folder (id, name) VALUES ($1, $2)", id, name);
      } else {
        txn.exec_params("INSERT INTO " + qsch + ".folder (id, parent_id, name) VALUES ($1, $2, $3)", id, parent_id, name);
      }
      txn.commit();
      return {true, ""};
    } catch (const pqxx::sql_error &e) {
      return {false, sql_err_msg(e)};
    } catch (const std::exception &e) {
      return {false, e.what()};
    }
  }

  std::pair<bool, std::string> rename_folder(const Conn &c, const std::string &schema, const std::string &id, const std::string &new_name)
  {
    try {
      pqxx::connection pg(make_cs(c));
      pqxx::work       txn(pg);
      txn.exec_params("UPDATE " + pg.quote_name(schema) + ".folder SET name = $1 WHERE id = $2", new_name, id);
      txn.commit();
      return {true, ""};
    } catch (const pqxx::sql_error &e) {
      return {false, sql_err_msg(e)};
    } catch (const std::exception &e) {
      return {false, e.what()};
    }
  }

  std::pair<bool, std::string> delete_folder_recursive(const Conn &c, const std::string &schema, const std::string &id)
  {
    try {
      pqxx::connection pg(make_cs(c));
      pqxx::work       txn(pg);
      std::string      qsch = pg.quote_name(schema);

      txn.exec_params("WITH RECURSIVE descendants AS ("
                      "  SELECT id FROM " +
                          qsch +
                          ".folder WHERE id = $1 "
                          "  UNION ALL "
                          "  SELECT f.id FROM " +
                          qsch +
                          ".folder f "
                          "  JOIN descendants d ON f.parent_id = d.id"
                          ") "
                          "DELETE FROM " +
                          qsch + ".folder WHERE id IN (SELECT id FROM descendants)",
                      id);
      txn.commit();
      return {true, ""};
    } catch (const pqxx::sql_error &e) {
      return {false, sql_err_msg(e)};
    } catch (const std::exception &e) {
      return {false, e.what()};
    }
  }

} // namespace back
