#include "ConnTest.h"
#include "CustomId.h"
#include <pqxx/pqxx>
#include <tuple>

static std::string make_cs(const Conn &c)
{
  std::string cs = "host=" + c.host;
  cs += " port=" + (c.port.empty() ? "5432" : c.port);
  cs += " dbname=" + (c.dbname.empty() ? "postgres" : c.dbname);
  if (!c.user.empty()) cs += " user=" + c.user;
  if (!c.pass.empty()) cs += " password=" + c.pass;
  cs += " connect_timeout=5";
  return cs;
}

static std::string sql_err_msg(const pqxx::sql_error &e)
{
  std::string msg = e.what();
  if (!e.query().empty()) {
    msg += "\nSQL:\n";
    msg += e.query();
  }
  return msg;
}

// Build folder tree from flat list (parent_id == "" means root)
static std::vector<FolderNode> build_folder_tree(
    const std::vector<std::tuple<std::string, std::string, std::string>> &flat,
    const std::string &parent_id)
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
static std::vector<FolderNode> load_folders_for_schema(
    pqxx::work &txn, pqxx::connection &pg, const std::string &sch)
{
  // Check if folder table exists
  auto check = txn.exec_params(
      "SELECT 1 FROM information_schema.tables "
      "WHERE table_schema = $1 AND table_name = 'folder' LIMIT 1", sch);
  if (check.empty()) return {};

  auto rows = txn.exec(
      "SELECT id, COALESCE(parent_id, ''), name "
      "FROM " + pg.quote_name(sch) + ".folder ORDER BY name");

  std::vector<std::tuple<std::string, std::string, std::string>> flat;
  for (const auto &row : rows)
    flat.emplace_back(row[0].c_str(), row[1].c_str(), row[2].c_str());
  return build_folder_tree(flat, "");
}

// ── Public API ────────────────────────────────────────────────────────────────

std::pair<bool, std::string> test_connection(
    const std::string &host, const std::string &port,
    const std::string &dbname, const std::string &user, const std::string &pass)
{
  try {
    Conn tmp;
    tmp.host = host; tmp.port = port; tmp.dbname = dbname;
    tmp.user = user; tmp.pass = pass;
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

    auto schema_rows = txn.exec(
        "SELECT table_schema FROM information_schema.tables "
        "WHERE table_name = 'lang_setting' "
        "  AND table_schema NOT IN ('pg_catalog','information_schema') "
        "ORDER BY table_schema");

    for (const auto &row : schema_rows) {
      std::string sch = row[0].c_str();
      try {
        pqxx::result vr = txn.exec(
            "SELECT value FROM " + pg.quote_name(sch) +
            ".lang_setting WHERE name = 'name' LIMIT 1");
        if (!vr.empty() && !vr[0][0].is_null()) {
          auto folders = load_folders_for_schema(txn, pg, sch);
          repos.push_back({sch, vr[0][0].c_str(), std::move(folders)});
        }
      } catch (...) {}
    }
    txn.commit();
    return {true, ""};
  } catch (const std::exception &e) {
    return {false, e.what()};
  }
}

std::pair<bool, std::string> create_repository(
    const Conn &c, const std::string &schema, const std::string &repo_name)
{
  try {
    pqxx::connection pg(make_cs(c));
    pqxx::work       txn(pg);
    std::string      qsch = pg.quote_name(schema);

    txn.exec("CREATE SCHEMA IF NOT EXISTS " + qsch);
    txn.exec(
        "CREATE TABLE IF NOT EXISTS " + qsch + ".lang_setting "
        "(name varchar(150) PRIMARY KEY, value text)");
    txn.exec_params(
        "INSERT INTO " + qsch + ".lang_setting (name, value) "
        "VALUES ('name', $1) "
        "ON CONFLICT (name) DO UPDATE SET value = EXCLUDED.value",
        repo_name);
    txn.exec(
        "CREATE TABLE IF NOT EXISTS " + qsch + ".folder "
        "(id varchar(32) PRIMARY KEY, parent_id varchar(32), name text)");
    txn.commit();
    return {true, ""};
  } catch (const pqxx::sql_error &e) {
    return {false, sql_err_msg(e)};
  } catch (const std::exception &e) {
    return {false, e.what()};
  }
}

std::pair<bool, std::string> edit_repository(
    const Conn &c, const std::string &old_schema,
    const std::string &new_schema, const std::string &new_repo_name)
{
  try {
    pqxx::connection pg(make_cs(c));
    pqxx::work       txn(pg);

    if (old_schema != new_schema) {
      txn.exec("ALTER SCHEMA " + pg.quote_name(old_schema) +
               " RENAME TO " + pg.quote_name(new_schema));
    }
    txn.exec_params(
        "UPDATE " + pg.quote_name(new_schema) +
        ".lang_setting SET value = $1 WHERE name = 'name'",
        new_repo_name);
    txn.commit();
    return {true, ""};
  } catch (const pqxx::sql_error &e) {
    return {false, sql_err_msg(e)};
  } catch (const std::exception &e) {
    return {false, e.what()};
  }
}

std::pair<bool, std::string> load_repo_folders(
    const Conn &c, const std::string &schema, std::vector<FolderNode> &root_folders)
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

std::pair<bool, std::string> create_folder(
    const Conn &c, const std::string &schema,
    const std::string &parent_id, const std::string &name)
{
  try {
    pqxx::connection pg(make_cs(c));
    pqxx::work       txn(pg);
    std::string      qsch = pg.quote_name(schema);

    // Create table in case repo was added before folder feature was introduced
    txn.exec(
        "CREATE TABLE IF NOT EXISTS " + qsch + ".folder "
        "(id varchar(32) PRIMARY KEY, parent_id varchar(32), name text)");

    std::string id = new_id();
    if (parent_id.empty()) {
      txn.exec_params(
          "INSERT INTO " + qsch + ".folder (id, name) VALUES ($1, $2)", id, name);
    } else {
      txn.exec_params(
          "INSERT INTO " + qsch + ".folder (id, parent_id, name) VALUES ($1, $2, $3)",
          id, parent_id, name);
    }
    txn.commit();
    return {true, ""};
  } catch (const pqxx::sql_error &e) {
    return {false, sql_err_msg(e)};
  } catch (const std::exception &e) {
    return {false, e.what()};
  }
}

std::pair<bool, std::string> rename_folder(
    const Conn &c, const std::string &schema,
    const std::string &id, const std::string &new_name)
{
  try {
    pqxx::connection pg(make_cs(c));
    pqxx::work       txn(pg);
    txn.exec_params(
        "UPDATE " + pg.quote_name(schema) + ".folder SET name = $1 WHERE id = $2",
        new_name, id);
    txn.commit();
    return {true, ""};
  } catch (const pqxx::sql_error &e) {
    return {false, sql_err_msg(e)};
  } catch (const std::exception &e) {
    return {false, e.what()};
  }
}

std::pair<bool, std::string> delete_folder_recursive(
    const Conn &c, const std::string &schema, const std::string &id)
{
  try {
    pqxx::connection pg(make_cs(c));
    pqxx::work       txn(pg);
    std::string      qsch = pg.quote_name(schema);

    txn.exec_params(
        "WITH RECURSIVE descendants AS ("
        "  SELECT id FROM " + qsch + ".folder WHERE id = $1 "
        "  UNION ALL "
        "  SELECT f.id FROM " + qsch + ".folder f "
        "  JOIN descendants d ON f.parent_id = d.id"
        ") "
        "DELETE FROM " + qsch + ".folder WHERE id IN (SELECT id FROM descendants)",
        id);
    txn.commit();
    return {true, ""};
  } catch (const pqxx::sql_error &e) {
    return {false, sql_err_msg(e)};
  } catch (const std::exception &e) {
    return {false, e.what()};
  }
}
