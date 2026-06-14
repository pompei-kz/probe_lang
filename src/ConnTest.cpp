#include "ConnTest.h"
#include <pqxx/pqxx>

static std::string make_cs(const Conn &c)
{
  std::string cs = "host=" + c.host;
  cs += " port=" + (c.port.empty() ? "5432" : c.port);
  cs += " dbname=postgres";
  if (!c.user.empty()) cs += " user=" + c.user;
  if (!c.pass.empty()) cs += " password=" + c.pass;
  cs += " connect_timeout=5";
  return cs;
}

std::pair<bool, std::string> test_connection(
    const std::string &host,
    const std::string &port,
    const std::string &user,
    const std::string &pass)
{
  try {
    Conn tmp{"", host, port, user, pass};
    pqxx::connection c(make_cs(tmp));
    return {true, "Connected successfully"};
  } catch (const std::exception &e) {
    return {false, e.what()};
  }
}

std::pair<bool, std::string> connect_and_load(const Conn &c, std::vector<SchemaNode> &schemas)
{
  schemas.clear();
  try {
    pqxx::connection pg(make_cs(c));
    pqxx::work       txn(pg);

    // Find schemas that have a lang_setting table
    auto schema_rows = txn.exec(
        "SELECT table_schema FROM information_schema.tables "
        "WHERE table_name = 'lang_setting' "
        "  AND table_schema NOT IN ('pg_catalog','information_schema') "
        "ORDER BY table_schema"
    );

    for (const auto &row : schema_rows) {
      std::string sch = row[0].c_str();
      try {
        pqxx::result vr = txn.exec("SELECT value FROM " + pg.quote_name(sch) + ".lang_setting WHERE name = 'name' LIMIT 1");
        if (!vr.empty() && !vr[0][0].is_null()) {
          schemas.push_back({sch, vr[0][0].c_str()});
        }
      } catch (...) {
        // skip schemas we can't query
      }
    }
    txn.commit();
    return {true, ""};
  } catch (const std::exception &e) {
    return {false, e.what()};
  }
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

std::pair<bool, std::string> create_repository(
    const Conn        &c,
    const std::string &schema,
    const std::string &repo_name)
{
  try {
    pqxx::connection pg(make_cs(c));
    pqxx::work       txn(pg);
    auto             qsch = pg.quote_name(schema);

    txn.exec("CREATE SCHEMA IF NOT EXISTS " + qsch);
    txn.exec(
        "CREATE TABLE IF NOT EXISTS " + qsch + ".lang_setting "
        "(name varchar(150) PRIMARY KEY, value text)"
    );
    txn.exec_params(
        "INSERT INTO " + qsch + ".lang_setting (name, value) "
        "VALUES ('name', $1) "
        "ON CONFLICT (name) DO UPDATE SET value = EXCLUDED.value",
        repo_name
    );
    txn.commit();
    return {true, ""};
  } catch (const pqxx::sql_error &e) {
    return {false, sql_err_msg(e)};
  } catch (const std::exception &e) {
    return {false, e.what()};
  }
}
