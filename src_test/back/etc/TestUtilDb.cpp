#include "TestDb.h"
#include "back/etc/UtilDb.h"
#include "back/pool/PoolService.h"
#include <gtest/gtest.h>
#include <pqxx/pqxx>

#include <chrono>
#include <memory>
#include <string>
#include <thread>
#include <vector>

// ---------------------------------------------------------------------------
// Fixture: each test runs in its own schema named "<TestName>_<timestamp>".
// Every schema the test creates is recorded in `schemas` and dropped in
// TearDown on success (kept on failure for inspection). The timestamp suffix
// keeps every run unique, so repeated runs never collide.
// ---------------------------------------------------------------------------

class UtilDbTest : public testing::Test
{
protected:
  // Connection comes from the shared pool (PoolService); `pg` points at it.
  back::pool::Connection   poolConn;
  pqxx::connection        *pg = nullptr;
  std::string              schema;  // primary per-test schema name
  std::vector<std::string> schemas; // every schema this test creates; dropped in TearDown

  void SetUp() override
  {
    try {
      poolConn = back::pool::acquire(test_db::conn());
      pg       = &*poolConn;
    } catch (const std::exception &e) {
      GTEST_SKIP() << "test DB unavailable: " << e.what();
    }

    // Schema starts with the test name (for easy correlation) plus a unique,
    // sortable microsecond timestamp. Dropped in TearDown on success (kept on
    // failure for inspection).
    const ::testing::TestInfo *info = ::testing::UnitTest::GetInstance()->current_test_info();
    const auto us = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    schema        = std::string(info ? info->name() : "unknown") + "_" + std::to_string(us);
    schemas.push_back(schema);

    pqxx::work txn(*pg);
    txn.exec("CREATE SCHEMA " + txn.quote_name(schema));
    txn.commit();
  }

  // Record an additional schema the test creates so TearDown drops it too.
  std::string track_schema(const std::string &name)
  {
    schemas.push_back(name);
    return name;
  }

  void TearDown() override
  {
    if (pg && !::testing::Test::HasFailure()) {
      for (const std::string &s : schemas) {
        try {
          pqxx::work txn(*pg);
          txn.exec("DROP SCHEMA IF EXISTS " + txn.quote_name(s) + " CASCADE");
          txn.commit();
        } catch (const std::exception &) {
          // best-effort cleanup
        }
      }
    }
  }

  // Fully-qualified, quoted "schema"."table".
  std::string qual(const std::string &table) const { return pg->quote_name(schema) + "." + pg->quote_name(table); }

  void create_table(const std::string &name, const std::string &cols)
  {
    pqxx::work txn(*pg);
    txn.exec("CREATE TABLE " + qual(name) + " (" + cols + ")");
    txn.commit();
  }

  bool column_exists(const std::string &table, const std::string &col)
  {
    pqxx::work txn(*pg);
    auto       r = txn.exec("SELECT 1 FROM information_schema.columns "
                            "WHERE table_schema=$1 AND table_name=$2 AND column_name=$3",
                            pqxx::params{txn, schema, table, col});
    return !r.empty();
  }
};

// ---------------------------------------------------------------------------
// ensureCreatedAt
// ---------------------------------------------------------------------------

TEST_F(UtilDbTest, EnsureCreatedAtAddsColumn)
{
  create_table("t", "id int");

  {
    pqxx::work txn(*pg);
    //
    //
    back::ensureCreatedAt(txn, schema, "t");
    //
    //
    txn.commit();
  }

  EXPECT_TRUE(column_exists("t", "created_at"));
}

TEST_F(UtilDbTest, EnsureCreatedAtIsTimestampWithNowDefault)
{
  create_table("t", "id int");

  {
    pqxx::work txn(*pg);
    //
    //
    back::ensureCreatedAt(txn, schema, "t");
    //
    //
    txn.commit();
  }

  pqxx::work txn(*pg);
  auto       r = txn.exec("SELECT data_type, column_default FROM information_schema.columns "
                          "WHERE table_schema=$1 AND table_name='t' AND column_name='created_at'",
                          pqxx::params{txn, schema});
  ASSERT_EQ(r.size(), 1u);
  EXPECT_EQ(r[0]["data_type"].as<std::string>(), "timestamp without time zone");
  ASSERT_FALSE(r[0]["column_default"].is_null());
  EXPECT_NE(r[0]["column_default"].as<std::string>().find("now()"), std::string::npos);
}

TEST_F(UtilDbTest, EnsureCreatedAtPopulatedOnInsert)
{
  create_table("t", "id int");

  {
    pqxx::work txn(*pg);
    //
    //
    back::ensureCreatedAt(txn, schema, "t");
    //
    //
    txn.commit();
  }

  pqxx::work txn(*pg);
  txn.exec("INSERT INTO " + qual("t") + " (id) VALUES (1)");
  auto r = txn.exec("SELECT created_at FROM " + qual("t") + " WHERE id=1");
  ASSERT_EQ(r.size(), 1u);
  EXPECT_FALSE(r[0][0].is_null());
  txn.commit();
}

TEST_F(UtilDbTest, EnsureCreatedAtIsIdempotent)
{
  create_table("t", "id int");
  {
    pqxx::work txn(*pg);
    back::ensureCreatedAt(txn, schema, "t"); // first call
    txn.commit();
  }

  {
    pqxx::work txn(*pg);
    //
    //
    EXPECT_NO_THROW(back::ensureCreatedAt(txn, schema, "t"));
    //
    //
    txn.commit();
  }

  pqxx::work txn(*pg);
  auto       r = txn.exec("SELECT count(*) FROM information_schema.columns "
                          "WHERE table_schema=$1 AND table_name='t' AND column_name='created_at'",
                          pqxx::params{txn, schema});
  EXPECT_EQ(r[0][0].as<int>(), 1);
}

// ---------------------------------------------------------------------------
// ensureLastModifiedAt
// ---------------------------------------------------------------------------

TEST_F(UtilDbTest, EnsureLastModifiedAtAddsColumn)
{
  create_table("t", "id int primary key, val text");

  {
    pqxx::work txn(*pg);
    //
    //
    back::ensureLastModifiedAt(txn, schema, "t");
    //
    //
    txn.commit();
  }

  EXPECT_TRUE(column_exists("t", "last_modified_at"));
}

TEST_F(UtilDbTest, LastModifiedAtNullBeforeUpdate)
{
  create_table("t", "id int primary key, val text");

  {
    pqxx::work txn(*pg);
    //
    //
    back::ensureLastModifiedAt(txn, schema, "t");
    //
    //
    txn.commit();
  }

  {
    pqxx::work txn(*pg);
    txn.exec("INSERT INTO " + qual("t") + " (id, val) VALUES (1, 'a')");
    txn.commit();
  }

  // Trigger fires only BEFORE UPDATE, so a freshly inserted row has no stamp.
  pqxx::work txn(*pg);
  auto       r = txn.exec("SELECT last_modified_at FROM " + qual("t") + " WHERE id=1");
  ASSERT_EQ(r.size(), 1u);
  EXPECT_TRUE(r[0][0].is_null());
}

TEST_F(UtilDbTest, LastModifiedAtSetWhenAnotherFieldChanges)
{
  create_table("t", "id int primary key, val text");

  {
    pqxx::work txn(*pg);
    //
    //
    back::ensureLastModifiedAt(txn, schema, "t");
    //
    //
    txn.commit();
  }

  {
    pqxx::work txn(*pg);
    txn.exec("INSERT INTO " + qual("t") + " (id, val) VALUES (1, 'a')");
    txn.commit();
  }
  {
    pqxx::work txn(*pg);
    txn.exec("UPDATE " + qual("t") + " SET val='b' WHERE id=1");
    txn.commit();
  }

  pqxx::work txn(*pg);
  auto       r = txn.exec("SELECT last_modified_at FROM " + qual("t") + " WHERE id=1");
  ASSERT_EQ(r.size(), 1u);
  EXPECT_FALSE(r[0][0].is_null());
}

TEST_F(UtilDbTest, LastModifiedAtAdvancesOnSubsequentUpdate)
{
  create_table("t", "id int primary key, val text");

  {
    pqxx::work txn(*pg);
    //
    //
    back::ensureLastModifiedAt(txn, schema, "t");
    //
    //
    txn.commit();
  }

  {
    pqxx::work txn(*pg);
    txn.exec("INSERT INTO " + qual("t") + " (id, val) VALUES (1, 'a')");
    txn.commit();
  }

  double first;
  {
    pqxx::work txn(*pg);
    txn.exec("UPDATE " + qual("t") + " SET val='b' WHERE id=1");
    first = txn.exec("SELECT extract(epoch from last_modified_at) FROM " + qual("t") + " WHERE id=1")[0][0].as<double>();
    txn.commit();
  }

  // Each UPDATE runs in its own transaction; now() == transaction start time,
  // so a real gap between transactions is needed to observe a later stamp.
  std::this_thread::sleep_for(std::chrono::milliseconds(50));

  double second;
  {
    pqxx::work txn(*pg);
    txn.exec("UPDATE " + qual("t") + " SET val='c' WHERE id=1");
    second = txn.exec("SELECT extract(epoch from last_modified_at) FROM " + qual("t") + " WHERE id=1")[0][0].as<double>();
    txn.commit();
  }

  EXPECT_GT(second, first);
}

TEST_F(UtilDbTest, EnsureLastModifiedAtIsIdempotent)
{
  create_table("t", "id int primary key, val text");
  {
    pqxx::work txn(*pg);
    back::ensureLastModifiedAt(txn, schema, "t"); // first call
    txn.commit();
  }

  // Second call must not throw (trigger/function are re-created cleanly).
  {
    pqxx::work txn(*pg);
    //
    //
    EXPECT_NO_THROW(back::ensureLastModifiedAt(txn, schema, "t"));
    //
    //
    txn.commit();
  }

  // Exactly one trigger remains on the table.
  pqxx::work txn(*pg);
  const auto r = txn.exec("SELECT count(*) FROM information_schema.triggers "
                          "WHERE trigger_schema=$1 AND event_object_table='t' "
                          "AND trigger_name='set_last_modified_at'",
                          pqxx::params{txn, schema});
  EXPECT_EQ(r[0][0].as<int>(), 1);
}

// ---------------------------------------------------------------------------
// hasSchema
// ---------------------------------------------------------------------------

TEST_F(UtilDbTest, HasSchemaTrueForExistingSchema)
{
  // The fixture's SetUp() already created `schema`.
  pqxx::work txn(*pg);
  //
  //
  const bool result = back::hasSchema(txn, schema);
  //
  //
  EXPECT_TRUE(result);
}

TEST_F(UtilDbTest, HasSchemaFalseForMissingSchema)
{
  pqxx::work txn(*pg);
  //
  //
  const bool result = back::hasSchema(txn, schema + "_does_not_exist");
  //
  //
  EXPECT_FALSE(result);
}

TEST_F(UtilDbTest, HasSchemaTrueForBuiltinPublicSchema)
{
  pqxx::work txn(*pg);
  //
  //
  const bool result = back::hasSchema(txn, "public");
  //
  //
  EXPECT_TRUE(result);
}

// ---------------------------------------------------------------------------
// hasTable
// ---------------------------------------------------------------------------

TEST_F(UtilDbTest, HasTableTrueForExistingTable)
{
  create_table("t", "id int");

  pqxx::work txn(*pg);
  //
  //
  const bool result = back::hasTable(txn, schema, "t");
  //
  //
  EXPECT_TRUE(result);
}

TEST_F(UtilDbTest, HasTableFalseForMissingTable)
{
  pqxx::work txn(*pg);
  //
  //
  const bool result = back::hasTable(txn, schema, "no_such_table");
  //
  //
  EXPECT_FALSE(result);
}

TEST_F(UtilDbTest, HasTableFalseWhenTableInDifferentSchema)
{
  // Table `t` lives in `schema`, so it must not be found via `public`.
  create_table("t", "id int");

  pqxx::work txn(*pg);
  //
  //
  const bool result = back::hasTable(txn, "public", "t");
  //
  //
  EXPECT_FALSE(result);
}

TEST_F(UtilDbTest, HasTableFalseForMissingSchema)
{
  pqxx::work txn(*pg);
  //
  //
  const bool result = back::hasTable(txn, schema + "_does_not_exist", "t");
  //
  //
  EXPECT_FALSE(result);
}

// ---------------------------------------------------------------------------
// hasIndex
// ---------------------------------------------------------------------------

TEST_F(UtilDbTest, HasIndexTrueForExistingIndex)
{
  create_table("t", "id int, val text");
  {
    pqxx::work txn(*pg);
    txn.exec("CREATE INDEX t_val_idx ON " + qual("t") + " (val)");
    txn.commit();
  }

  pqxx::work txn(*pg);
  //
  //
  const bool result = back::hasIndex(txn, schema, "t_val_idx");
  //
  //
  EXPECT_TRUE(result);
}

TEST_F(UtilDbTest, HasIndexFalseForMissingIndex)
{
  create_table("t", "id int, val text");

  pqxx::work txn(*pg);
  //
  //
  const bool result = back::hasIndex(txn, schema, "no_such_index");
  //
  //
  EXPECT_FALSE(result);
}

TEST_F(UtilDbTest, HasIndexFalseWhenIndexInDifferentSchema)
{
  // The index lives in `schema`, so it must not be found via `public`.
  create_table("t", "id int, val text");
  {
    pqxx::work txn(*pg);
    txn.exec("CREATE INDEX t_val_idx ON " + qual("t") + " (val)");
    txn.commit();
  }

  pqxx::work txn(*pg);
  //
  //
  const bool result = back::hasIndex(txn, "public", "t_val_idx");
  //
  //
  EXPECT_FALSE(result);
}

TEST_F(UtilDbTest, HasIndexTrueForPrimaryKeyIndex)
{
  // A PRIMARY KEY creates an implicit index named "<table>_pkey".
  create_table("t", "id int primary key, val text");

  pqxx::work txn(*pg);
  //
  //
  const bool result = back::hasIndex(txn, schema, "t_pkey");
  //
  //
  EXPECT_TRUE(result);
}

TEST_F(UtilDbTest, HasIndexFalseForMissingSchema)
{
  pqxx::work txn(*pg);
  //
  //
  const bool result = back::hasIndex(txn, schema + "_does_not_exist", "t_val_idx");
  //
  //
  EXPECT_FALSE(result);
}

// ---------------------------------------------------------------------------
// hasSequence
// ---------------------------------------------------------------------------

TEST_F(UtilDbTest, HasSequenceTrueForExistingSequence)
{
  {
    pqxx::work txn(*pg);
    txn.exec("CREATE SEQUENCE " + qual("s"));
    txn.commit();
  }

  pqxx::work txn(*pg);
  //
  //
  const bool result = back::hasSequence(txn, schema, "s");
  //
  //
  EXPECT_TRUE(result);
}

TEST_F(UtilDbTest, HasSequenceFalseForMissingSequence)
{
  pqxx::work txn(*pg);
  //
  //
  const bool result = back::hasSequence(txn, schema, "no_such_sequence");
  //
  //
  EXPECT_FALSE(result);
}

TEST_F(UtilDbTest, HasSequenceFalseWhenSequenceInDifferentSchema)
{
  // The sequence lives in `schema`, so it must not be found via `public`.
  {
    pqxx::work txn(*pg);
    txn.exec("CREATE SEQUENCE " + qual("s"));
    txn.commit();
  }

  pqxx::work txn(*pg);
  //
  //
  const bool result = back::hasSequence(txn, "public", "s");
  //
  //
  EXPECT_FALSE(result);
}

TEST_F(UtilDbTest, HasSequenceFalseForMissingSchema)
{
  pqxx::work txn(*pg);
  //
  //
  const bool result = back::hasSequence(txn, schema + "_does_not_exist", "s");
  //
  //
  EXPECT_FALSE(result);
}
