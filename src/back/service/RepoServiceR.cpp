#include "back/service/RepoServiceR.h"
#include "back/service/FolderServiceR.h"
#include "back/service/UnitServiceR.h"

namespace back {

  using namespace model;

  // Find a folder by id anywhere in the tree.
  static FolderNode *find_folder(std::vector<FolderNode> &folders, const std::string &id)
  {
    for (auto &f : folders) {
      if (f.id == id) return &f;
      if (FolderNode *found = find_folder(f.children, id)) return found;
    }
    return nullptr;
  }

  // Load the folder tree + units for a schema, attaching each unit to its
  // parent folder (or to the repo root when parent_folder_id is empty).
  static void load_tree(
      pqxx::work &txn, pqxx::connection &pg, const std::string &sch, std::vector<FolderNode> &folders, std::vector<Unit> &root_units)
  {
    folders = load_folders_for_schema(txn, pg, sch);
    root_units.clear();
    for (auto &u : load_units_for_schema(txn, pg, sch)) {
      if (u.parent_folder_id.empty()) {
        root_units.push_back(std::move(u));
      } else if (FolderNode *f = find_folder(folders, u.parent_folder_id)) {
        f->units.push_back(std::move(u));
      }
      // units pointing at a missing folder are silently dropped
    }
  }

  std::string make_cs(const Conn &c)
  {
    std::string cs = "host=" + c.host;
    cs += " port=" + (c.port.empty() ? "5432" : c.port);
    cs += " dbname=" + (c.dbname.empty() ? "postgres" : c.dbname);
    if (!c.user.empty()) cs += " user=" + c.user;
    if (!c.pass.empty()) cs += " password=" + c.pass;
    cs += " connect_timeout=5";
    return cs;
  }

  std::string sql_err_msg(const pqxx::sql_error &e)
  {
    std::string msg = e.what();
    if (!e.query().empty()) {
      msg += "\nSQL:\n";
      msg += e.query();
    }
    return msg;
  }

  // ── Public API ────────────────────────────────────────────────────────────────

  std::pair<bool, std::string> test_connection(
      const std::string &host, const std::string &port, const std::string &dbname, const std::string &user, const std::string &pass)
  {
    try {
      Conn tmp;
      tmp.host   = host;
      tmp.port   = port;
      tmp.dbname = dbname;
      tmp.user   = user;
      tmp.pass   = pass;
      pqxx::connection c(make_cs(tmp));
      return {true, "Connected successfully"};
    } catch (const std::exception &e) {
      return {false, e.what()};
    }
  }

  std::pair<bool, std::string> connect_and_load(const Conn &c, std::vector<RepoNode> &repos)
  {
    repos.clear();
    try {
      pqxx::connection pg(make_cs(c));
      pqxx::work       txn(pg);

      auto schema_rows = txn.exec("SELECT table_schema FROM information_schema.tables "
                                  "WHERE table_name = 'lang_setting' "
                                  "  AND table_schema NOT IN ('pg_catalog','information_schema') "
                                  "ORDER BY table_schema");

      for (const auto &row : schema_rows) {
        std::string sch = row[0].c_str();
        try {
          pqxx::result vr = txn.exec("SELECT value FROM " + pg.quote_name(sch) + ".lang_setting WHERE name = 'name' LIMIT 1");
          if (!vr.empty() && !vr[0][0].is_null()) {
            RepoNode rn;
            rn.schema_name = sch;
            rn.repo_name   = vr[0][0].c_str();
            load_tree(txn, pg, sch, rn.folders, rn.units);
            repos.push_back(std::move(rn));
          }
        } catch (...) {
        }
      }
      txn.commit();
      return {true, ""};
    } catch (const std::exception &e) {
      return {false, e.what()};
    }
  }

  std::pair<bool, std::string> load_repo_tree(const Conn &c, const std::string &schema, RepoNode &repo)
  {
    repo.folders.clear();
    repo.units.clear();
    try {
      pqxx::connection pg(make_cs(c));
      pqxx::work       txn(pg);
      load_tree(txn, pg, schema, repo.folders, repo.units);
      txn.commit();
      return {true, ""};
    } catch (const std::exception &e) {
      return {false, e.what()};
    }
  }

} // namespace back
