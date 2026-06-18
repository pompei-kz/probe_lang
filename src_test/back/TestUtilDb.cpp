#include "TestDb.h"
#include "back/UtilDb.h"
#include <gtest/gtest.h>
#include <pqxx/pqxx>

#include <chrono>
#include <memory>
#include <string>
#include <thread>

using namespace back;

// ---------------------------------------------------------------------------
// Fixture: each test runs in its own schema named "<TestName>_<timestamp>".
// The schema is deliberately left in place after the test (no cleanup) so the
// resulting DB state can be inspected afterwards. The timestamp suffix keeps
// every run unique, so repeated runs never collide.
// ---------------------------------------------------------------------------

class UtilDbTest : public testing::Test
{
protected:
  std::unique_ptr<pqxx::connection> pg;
  std::string                       schema;

  void SetUp() override
  {
    try {
      pg = std::make_unique<pqxx::connection>(test_db::dsn());
    } catch (const std::exception &e) {
      GTEST_SKIP() << "test DB unavailable: " << e.what();
    }

    // Schema starts with the test name (for easy correlation) plus a unique,
    // sortable microsecond timestamp. Not dropped afterwards by design.
    const ::testing::TestInfo *info = ::testing::UnitTest::GetInstance()->current_test_info();
    const auto us = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    schema        = std::string(info ? info->name() : "unknown") + "_" + std::to_string(us);

    pqxx::work txn(*pg);
    txn.exec("CREATE SCHEMA " + txn.quote_name(schema));
    txn.commit();
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
    auto       r = txn.exec_params("SELECT 1 FROM information_schema.columns "
                                   "WHERE table_schema=$1 AND table_name=$2 AND column_name=$3",
                                   schema,
                                   table,
                                   col);
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
    ensureCreatedAt(txn, schema, "t");
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
    ensureCreatedAt(txn, schema, "t");
    //
    //
    txn.commit();
  }

  pqxx::work txn(*pg);
  auto       r = txn.exec_params("SELECT data_type, column_default FROM information_schema.columns "
                                 "WHERE table_schema=$1 AND table_name='t' AND column_name='created_at'",
                                 schema);
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
    ensureCreatedAt(txn, schema, "t");
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
    ensureCreatedAt(txn, schema, "t"); // first call
    txn.commit();
  }

  {
    pqxx::work txn(*pg);
    //
    //
    EXPECT_NO_THROW(ensureCreatedAt(txn, schema, "t"));
    //
    //
    txn.commit();
  }

  pqxx::work txn(*pg);
  auto       r = txn.exec_params("SELECT count(*) FROM information_schema.columns "
                                 "WHERE table_schema=$1 AND table_name='t' AND column_name='created_at'",
                                 schema);
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
    ensureLastModifiedAt(txn, schema, "t");
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
    ensureLastModifiedAt(txn, schema, "t");
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
    ensureLastModifiedAt(txn, schema, "t");
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
    ensureLastModifiedAt(txn, schema, "t");
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
    ensureLastModifiedAt(txn, schema, "t"); // first call
    txn.commit();
  }

  // Second call must not throw (trigger/function are re-created cleanly).
  {
    pqxx::work txn(*pg);
    //
    //
    EXPECT_NO_THROW(ensureLastModifiedAt(txn, schema, "t"));
    //
    //
    txn.commit();
  }

  // Exactly one trigger remains on the table.
  pqxx::work txn(*pg);
  const auto r = txn.exec_params("SELECT count(*) FROM information_schema.triggers "
                                 "WHERE trigger_schema=$1 AND event_object_table='t' "
                                 "AND trigger_name='set_last_modified_at'",
                                 schema);
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
  const bool result = hasSchema(txn, schema);
  //
  //
  EXPECT_TRUE(result);
}

TEST_F(UtilDbTest, HasSchemaFalseForMissingSchema)
{
  pqxx::work txn(*pg);
  //
  //
  const bool result = hasSchema(txn, schema + "_does_not_exist");
  //
  //
  EXPECT_FALSE(result);
}

TEST_F(UtilDbTest, HasSchemaTrueForBuiltinPublicSchema)
{
  pqxx::work txn(*pg);
  //
  //
  const bool result = hasSchema(txn, "public");
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
  const bool result = hasTable(txn, schema, "t");
  //
  //
  EXPECT_TRUE(result);
}

TEST_F(UtilDbTest, HasTableFalseForMissingTable)
{
  pqxx::work txn(*pg);
  //
  //
  const bool result = hasTable(txn, schema, "no_such_table");
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
  const bool result = hasTable(txn, "public", "t");
  //
  //
  EXPECT_FALSE(result);
}

TEST_F(UtilDbTest, HasTableFalseForMissingSchema)
{
  pqxx::work txn(*pg);
  //
  //
  const bool result = hasTable(txn, schema + "_does_not_exist", "t");
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
  const bool result = hasIndex(txn, schema, "t_val_idx");
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
  const bool result = hasIndex(txn, schema, "no_such_index");
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
  const bool result = hasIndex(txn, "public", "t_val_idx");
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
  const bool result = hasIndex(txn, schema, "t_pkey");
  //
  //
  EXPECT_TRUE(result);
}

TEST_F(UtilDbTest, HasIndexFalseForMissingSchema)
{
  pqxx::work txn(*pg);
  //
  //
  const bool result = hasIndex(txn, schema + "_does_not_exist", "t_val_idx");
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
  const bool result = hasSequence(txn, schema, "s");
  //
  //
  EXPECT_TRUE(result);
}

TEST_F(UtilDbTest, HasSequenceFalseForMissingSequence)
{
  pqxx::work txn(*pg);
  //
  //
  const bool result = hasSequence(txn, schema, "no_such_sequence");
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
  const bool result = hasSequence(txn, "public", "s");
  //
  //
  EXPECT_FALSE(result);
}

TEST_F(UtilDbTest, HasSequenceFalseForMissingSchema)
{
  pqxx::work txn(*pg);
  //
  //
  const bool result = hasSequence(txn, schema + "_does_not_exist", "s");
  //
  //
  EXPECT_FALSE(result);
}
