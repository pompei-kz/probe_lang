#include "InitDb.h"
#include "UtilDb.h"

namespace back {

  // Column definition for the folder table, shared by both creation paths.
  static constexpr const char *FOLDER_TABLE_DEF = "(id varchar(32) PRIMARY KEY, parent_id varchar(32), name text)";

  // Column definition for the unit table. No foreign keys; type is one of
  // Class / Interface / Enum, enforced by a CHECK constraint.
  static constexpr const char *UNIT_TABLE_DEF = "(id varchar(32) PRIMARY KEY, parent_folder_id varchar(32), name text, "
                                                "type text CHECK (type IN ('Class','Interface','Enum')))";

  void init_folder_table(pqxx::work &txn, pqxx::connection &pg, const std::string &schema)
  {
    const std::string qsch = pg.quote_name(schema);
    txn.exec("CREATE TABLE IF NOT EXISTS " + qsch + ".folder " + FOLDER_TABLE_DEF);
    ensureCreatedAt(txn, schema, "folder");
    ensureLastModifiedAt(txn, schema, "folder");
  }

  void init_unit_table(pqxx::work &txn, pqxx::connection &pg, const std::string &schema)
  {
    const std::string qsch = pg.quote_name(schema);
    txn.exec("CREATE TABLE IF NOT EXISTS " + qsch + ".unit " + UNIT_TABLE_DEF);
    ensureCreatedAt(txn, schema, "unit");
    ensureLastModifiedAt(txn, schema, "unit");
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
    init_unit_table(txn, pg, schema);
  }

} // namespace back
