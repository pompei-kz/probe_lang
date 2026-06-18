#include "InitDb.h"
#include "UtilDb.h"

namespace back {

  InitDb::InitDb(pqxx::work &txn, pqxx::connection &pg, const std::string &schema) : txn_(txn), pg_(pg), schema_(schema) {}

  // Column definition for the folder table, shared by both creation paths.
  static constexpr const char *FOLDER_TABLE_DEF = "(id varchar(32) PRIMARY KEY, parent_id varchar(32), name text)";

  void InitDb::init_folder_table() const
  {
    const std::string schemaQuote = pg_.quote_name(schema_);

    txn_.exec("CREATE TABLE IF NOT EXISTS " + schemaQuote + ".folder " + FOLDER_TABLE_DEF);

    ensureCreatedAt(txn_, schema_, "folder");
    ensureLastModifiedAt(txn_, schema_, "folder");
  }

  // Column definition for the unit table. No foreign keys; type is one of
  // Class / Interface / Enum, enforced by a CHECK constraint.
  static constexpr const char *UNIT_TABLE_DEF = "(id varchar(32) PRIMARY KEY,"
                                                " parent_folder_id varchar(32),"
                                                " name text,"
                                                " type text CHECK (type IN ('Class','Interface','Enum'))"
                                                ")";

  void InitDb::init_unit_table() const
  {
    const std::string schemaQuote = pg_.quote_name(schema_);
    txn_.exec("CREATE TABLE IF NOT EXISTS " + schemaQuote + ".unit " + UNIT_TABLE_DEF);
    ensureCreatedAt(txn_, schema_, "unit");
    ensureLastModifiedAt(txn_, schema_, "unit");

    init_unit_b_tables();
  }

  void InitDb::init_repo_schema() const
  {
    const std::string schemaQuote = pg_.quote_name(schema_);

    txn_.exec("CREATE SCHEMA IF NOT EXISTS " + schemaQuote);
    txn_.exec("CREATE TABLE IF NOT EXISTS " + schemaQuote +
              ".lang_setting "
              "(name varchar(150) PRIMARY KEY, value text)");

    ensureCreatedAt(txn_, schema_, "lang_setting");
    ensureLastModifiedAt(txn_, schema_, "lang_setting");

    init_folder_table();
    init_unit_table();
    init_unit_b_tables();
    init_unit_e_tables();
  }

  void InitDb::init_unit_b_tables() const
  {
    const std::string schemaQuoted = pg_.quote_name(schema_);

    if (!hasSchema(txn_, schema_)) {
      txn_.exec("CREATE SCHEMA " + schemaQuoted);
    }

    if (!hasTable(txn_, schema_, "unit_b")) {
      txn_.exec("CREATE TABLE " + schemaQuoted +
                ".unit_b "
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

    if (!hasIndex(txn_, schema_, "unit_b_geom")) {
      txn_.exec("CREATE INDEX unit_b_geom ON " + schemaQuoted + ".unit_b USING GIST(geom)");
    }

    ensureCreatedAt(txn_, schema_, "unit_b");
    ensureLastModifiedAt(txn_, schema_, "unit_b");

    if (!hasTable(txn_, schema_, "unit_b_method")) {
      txn_.exec("CREATE TABLE " + schemaQuoted +
                ".unit_b_method "
                "("
                "  id           varchar(32) primary key,"
                "  type         text CHECK (type IN ('Inner','Static','Constructor','Destructor')) default 'Inner',"
                "  access       text CHECK (access IN ('Public','Protected','Private')) default 'Private',"
                "  next_unit_id varchar(32),"
                "  disabled     bool default false,"
                "  name         text"
                ")");

      ensureCreatedAt(txn_, schema_, "unit_b_method");
      ensureLastModifiedAt(txn_, schema_, "unit_b_method");
    }

    if (!hasTable(txn_, schema_, "unit_b_method_arg")) {
      txn_.exec("CREATE TABLE " + schemaQuoted +
                ".unit_b_method_arg "
                "("
                "  id              varchar(32) primary key,"
                "  owner_method_id varchar(32) not null,"
                "  order_index     float8 not null,"
                "  name            text"
                ")");

      ensureCreatedAt(txn_, schema_, "unit_b_method_arg");
      ensureLastModifiedAt(txn_, schema_, "unit_b_method_arg");
    }

    if (!hasTable(txn_, schema_, "unit_b_field")) {
      txn_.exec("CREATE TABLE " + schemaQuoted +
                ".unit_b_field "
                "("
                "  id varchar(32) primary key,"
                "  next_unit_id varchar(32),"
                "  access       text CHECK (access IN ('Public','Protected','Private')) default 'Private',"
                "  commented    bool default false,"
                "  disabled     bool default false,"
                "  name text"
                ")");

      ensureCreatedAt(txn_, schema_, "unit_b_field");
      ensureLastModifiedAt(txn_, schema_, "unit_b_field");
    }
  }

  void InitDb::init_unit_e_tables() const
  {
    const std::string schemaQuoted = pg_.quote_name(schema_);
    if (!hasTable(txn_, schema_, "unit_e")) {
      txn_.exec("CREATE TABLE " + schemaQuoted +
                ".unit_e ("
                "  id        VARCHAR(32) primary key,"
                "  unit_id   VARCHAR(32) not null,"
                "  type      VARCHAR(150) not null,"
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

    if (!hasIndex(txn_, schema_, "unit_b_geom")) {
      txn_.exec("CREATE INDEX unit_e_geom ON " + schemaQuoted + ".unit_b USING GIST(geom)");
    }

    ensureCreatedAt(txn_, schema_, "unit_e");
    ensureLastModifiedAt(txn_, schema_, "unit_e");
  }

} // namespace back
