#include "UtilDb.h"

namespace back {

  void ensureCreatedAt(pqxx::work &txn, const std::string &schemaName, const std::string &tableName)
  {
    const std::string qtable = txn.quote_name(schemaName) + "." + txn.quote_name(tableName);
    txn.exec("ALTER TABLE " + qtable + " ADD COLUMN IF NOT EXISTS created_at timestamp DEFAULT now()");
  }

  void ensureLastModifiedAt(pqxx::work &txn, const std::string &schemaName, const std::string &tableName)
  {
    const std::string qSchema = txn.quote_name(schemaName);
    const std::string qTable  = qSchema + "." + txn.quote_name(tableName);

    txn.exec("ALTER TABLE " + qTable + " ADD COLUMN IF NOT EXISTS last_modified_at timestamp");

    // Trigger function (one per schema) that stamps the column on every update.
    txn.exec("CREATE OR REPLACE FUNCTION " + qSchema +
             ".set_last_modified_at() RETURNS trigger "
             "LANGUAGE PlPgSql AS $$ "
             "BEGIN NEW.last_modified_at = now(); RETURN NEW; END; $$");

    // Re-create the trigger idempotently (CREATE TRIGGER has no IF NOT EXISTS
    // on all supported server versions).
    txn.exec("DROP TRIGGER IF EXISTS set_last_modified_at ON " + qTable);
    txn.exec("CREATE TRIGGER set_last_modified_at BEFORE UPDATE ON " + qTable + " FOR EACH ROW EXECUTE FUNCTION " + qSchema +
             ".set_last_modified_at()");
  }

  bool hasSchema(pqxx::work &txn, const std::string &schemaName)
  {
    pqxx::result check = txn.exec_params("SELECT 1 FROM information_schema.schemata "
                                         "WHERE schema_name = $1 LIMIT 1",
                                         schemaName);
    return !check.empty();
  }

  bool hasTable(pqxx::work &txn, const std::string &schemaName, const std::string &tableName)
  {
    pqxx::result check = txn.exec_params("SELECT 1 FROM information_schema.tables "
                                         "WHERE table_schema = $1 AND table_name = $2 LIMIT 1",
                                         schemaName,
                                         tableName);
    return !check.empty();
  }

  bool hasSequence(pqxx::work &txn, const std::string &schemaName, const std::string &sequenceName)
  {
    pqxx::result check = txn.exec_params("SELECT 1 FROM information_schema.sequences "
                                         "WHERE sequence_schema = $1 AND sequence_name = $2 LIMIT 1",
                                         schemaName,
                                         sequenceName);
    return !check.empty();
  }

  bool hasIndex(pqxx::work &txn, const std::string &schemaName, const std::string &indexName)
  {
    pqxx::result check = txn.exec_params("SELECT 1 FROM pg_indexes "
                                         "WHERE schemaname = $1 AND indexname = $2 LIMIT 1",
                                         schemaName,
                                         indexName);
    return !check.empty();
  }

} // namespace back
