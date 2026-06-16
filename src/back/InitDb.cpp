#include "InitDb.h"
#include "UtilDb.h"

namespace back {

  // Column definition for the folder table, shared by both creation paths.
  static constexpr const char *FOLDER_TABLE_DEF = "(id varchar(32) PRIMARY KEY, parent_id varchar(32), name text)";

  void init_folder_table(pqxx::work &txn, pqxx::connection &pg, const std::string &schema)
  {
    const std::string schemaQuote = pg.quote_name(schema);

    txn.exec("CREATE TABLE IF NOT EXISTS " + schemaQuote + ".folder " + FOLDER_TABLE_DEF);

    ensureCreatedAt(txn, schema, "folder");
    ensureLastModifiedAt(txn, schema, "folder");
  }

  // Column definition for the unit table. No foreign keys; type is one of
  // Class / Interface / Enum, enforced by a CHECK constraint.
  static constexpr const char *UNIT_TABLE_DEF = "(id varchar(32) PRIMARY KEY,"
                                                " parent_folder_id varchar(32),"
                                                " name text,"
                                                " type text CHECK (type IN ('Class','Interface','Enum'))"
                                                ")";

  void init_unit_table(pqxx::work &txn, pqxx::connection &pg, const std::string &schema)
  {
    const std::string schemaQuote = pg.quote_name(schema);
    txn.exec("CREATE TABLE IF NOT EXISTS " + schemaQuote + ".unit " + UNIT_TABLE_DEF);
    ensureCreatedAt(txn, schema, "unit");
    ensureLastModifiedAt(txn, schema, "unit");

    init_unit_st_tables(txn, pg, schema);
  }

  void init_repo_schema(pqxx::work &txn, pqxx::connection &pg, const std::string &schema)
  {
    const std::string schemaQuote = pg.quote_name(schema);

    txn.exec("CREATE SCHEMA IF NOT EXISTS " + schemaQuote);
    txn.exec("CREATE TABLE IF NOT EXISTS " + schemaQuote +
             ".lang_setting "
             "(name varchar(150) PRIMARY KEY, value text)");

    ensureCreatedAt(txn, schema, "lang_setting");
    ensureLastModifiedAt(txn, schema, "lang_setting");

    init_folder_table(txn, pg, schema);
    init_unit_table(txn, pg, schema);
    init_unit_st_tables(txn, pg, schema);
  }

  void init_unit_st_tables(pqxx::work &txn, pqxx::connection &pg, const std::string &schema)
  {
    const std::string schemaQuoted = pg.quote_name(schema);

    if (!hasSchema(txn, schema)) {
      txn.exec("CREATE SCHEMA " + schemaQuoted);
    }

    if (!hasTable(txn, schema, "unit_st")) {
      txn.exec("CREATE TABLE " + schemaQuoted +
               ".unit_st "
               "("
               "  id        VARCHAR(32) primary key,"
               "  unit_id   VARCHAR(32) not null,"
               "  type VARCHAR(150) not null,"
               "  x         FLOAT4 not null,"
               "  y         FLOAT4 not null,"
               "  width     FLOAT4 not null,"
               "  height    FLOAT4 not null,"
               "  geom GEOMETRY(Polygon, 0)"
               "  GENERATED ALWAYS AS ("
               "    ST_MakeEnvelope(x, y, x + width, y + height, 0)"
               "  ) STORED"
               ")");
    }

    if (!hasIndex(txn, schema, "unit_st_geom")) {
      txn.exec("CREATE INDEX unit_st_geom ON " + schemaQuoted + ".unit_st USING GIST(geom)");
    }

    ensureCreatedAt(txn, schema, "unit_st");
    ensureLastModifiedAt(txn, schema, "unit_st");

    if (!hasTable(txn, schema, "unit_st_method")) {
      txn.exec("CREATE TABLE " + schemaQuoted +
               ".unit_st_method "
               "("
               "  id varchar(32) primary key,"
               "  next_unit_id varchar(32),"
               "  commented bool default false,"
               "  name text"
               ")");

      ensureCreatedAt(txn, schema, "unit_st_method");
      ensureLastModifiedAt(txn, schema, "unit_st_method");
    }

    if (!hasTable(txn, schema, "unit_st_field")) {
      txn.exec("CREATE TABLE " + schemaQuoted +
               ".unit_st_field "
               "("
               "  id varchar(32) primary key,"
               "  next_unit_id varchar(32),"
               "  commented bool default false,"
               "  name text"
               ")");

      ensureCreatedAt(txn, schema, "unit_st_field");
      ensureLastModifiedAt(txn, schema, "unit_st_field");
    }
  }

} // namespace back
