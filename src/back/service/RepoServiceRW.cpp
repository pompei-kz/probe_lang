#include "back/service/RepoServiceRW.h"
#include "back/etc/InitDb.h"
#include "back/service/RepoServiceR.h"

namespace back {

  std::pair<bool, std::string> create_repository(const model::Conn &c, const std::string &schema, const std::string &repo_name)
  {
    try {
      pqxx::connection pg(make_cs(c));
      pqxx::work       txn(pg);
      std::string      qsch = pg.quote_name(schema);

      InitDb(txn, pg, schema).init_repo_schema();
      txn.exec("INSERT INTO " + qsch +
                   ".lang_setting (name, value) "
                   "VALUES ('name', $1) "
                   "ON CONFLICT (name) DO UPDATE SET value = EXCLUDED.value",
               pqxx::params{txn, repo_name});
      txn.commit();
      return {true, ""};
    } catch (const pqxx::sql_error &e) {
      return {false, sql_err_msg(e)};
    } catch (const std::exception &e) {
      return {false, e.what()};
    }
  }

  std::pair<bool, std::string> edit_repository(const model::Conn &c,
                                               const std::string &old_schema,
                                               const std::string &new_schema,
                                               const std::string &new_repo_name)
  {
    try {
      pqxx::connection pg(make_cs(c));
      pqxx::work       txn(pg);

      if (old_schema != new_schema) {
        txn.exec("ALTER SCHEMA " + pg.quote_name(old_schema) + " RENAME TO " + pg.quote_name(new_schema));
      }
      txn.exec("UPDATE " + pg.quote_name(new_schema) + ".lang_setting SET value = $1 WHERE name = 'name'", pqxx::params{txn, new_repo_name});
      txn.commit();
      return {true, ""};
    } catch (const pqxx::sql_error &e) {
      return {false, sql_err_msg(e)};
    } catch (const std::exception &e) {
      return {false, e.what()};
    }
  }

  std::pair<bool, std::string> ensure_repo_schema(const model::Conn &c, const std::string &schema)
  {
    try {
      pqxx::connection pg(make_cs(c));
      pqxx::work       txn(pg);
      InitDb(txn, pg, schema).init_repo_schema();
      txn.commit();
      return {true, ""};
    } catch (const pqxx::sql_error &e) {
      return {false, sql_err_msg(e)};
    } catch (const std::exception &e) {
      return {false, e.what()};
    }
  }

} // namespace back
