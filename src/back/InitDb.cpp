#include "InitDb.h"
#include "UtilDb.h"

namespace back {

  // Column definition for the folder table, shared by both creation paths.
  static constexpr const char *FOLDER_TABLE_DEF = "(id varchar(32) PRIMARY KEY, parent_id varchar(32), name text)";

  void init_folder_table(pqxx::work &txn, pqxx::connection &pg, const std::string &schema)
  {
    const std::string qsch = pg.quote_name(schema);
    txn.exec("CREATE TABLE IF NOT EXISTS " + qsch + ".folder " + FOLDER_TABLE_DEF);
  }

  void init_repo_schema(pqxx::work &txn, pqxx::connection &pg, const std::string &schema)
  {
    const std::string qsch = pg.quote_name(schema);
    txn.exec("CREATE SCHEMA IF NOT EXISTS " + qsch);
    txn.exec("CREATE TABLE IF NOT EXISTS " + qsch +
             ".lang_setting "
             "(name varchar(150) PRIMARY KEY, value text)");
    ensureCreatedAt(txn, schema, "lang_setting");
    ensureLastModifiedAt(txn, schema, "lang_setting");
    init_folder_table(txn, pg, schema);
  }

} // namespace back
