#include "DbTestBase.h"
#include "back/UndoService.h"
#include <gtest/gtest.h>

using namespace back;
using namespace back::model;

class UndoServiceTest : public DbTest
{
protected:
  // Run DDL/DML setup in its own committed transaction.
  void setup_sql(const std::string &sql)
  {
    pqxx::work txn(*pg);
    txn.exec(sql);
    txn.commit();
  }
};

TEST_F(UndoServiceTest, UpdateUndoCapturesOldColumnValue)
{
  make_schema();
  setup_sql("CREATE TABLE " + qual("t") + " (id varchar(32) primary key, val text)");
  setup_sql("INSERT INTO " + qual("t") + " (id, val) VALUES ('r1', 'old')");

  UndoRowChange change{"t", "r1", false, "val", "new"};

  pqxx::work  txn(*pg);
  UndoService svc(txn, *pg, schema);
  //
  //
  std::vector<UndoRowChange> undo = svc.collectUndoChanges({change});
  //
  //

  ASSERT_EQ(undo.size(), 1u);
  EXPECT_EQ(undo[0].tableName, "t");
  EXPECT_EQ(undo[0].idValue, "r1");
  EXPECT_FALSE(undo[0].toDelete);
  EXPECT_EQ(undo[0].colName, "val");
  ASSERT_TRUE(undo[0].value.has_value());
  EXPECT_EQ(*undo[0].value, "old");
}

TEST_F(UndoServiceTest, UpdateUndoOfNullOldValueIsNullopt)
{
  make_schema();
  setup_sql("CREATE TABLE " + qual("t") + " (id varchar(32) primary key, val text)");
  setup_sql("INSERT INTO " + qual("t") + " (id, val) VALUES ('r2', NULL)");

  UndoRowChange change{"t", "r2", false, "val", "x"};

  pqxx::work  txn(*pg);
  UndoService svc(txn, *pg, schema);
  //
  //
  std::vector<UndoRowChange> undo = svc.collectUndoChanges({change});
  //
  //

  ASSERT_EQ(undo.size(), 1u);
  EXPECT_FALSE(undo[0].value.has_value()); // restore back to NULL
}

TEST_F(UndoServiceTest, ChangesAreUndoneInReverseOrder)
{
  make_schema();
  setup_sql("CREATE TABLE " + qual("t") + " (id varchar(32) primary key, val text)");
  setup_sql("INSERT INTO " + qual("t") + " (id, val) VALUES ('a', '1'), ('b', '2')");

  UndoRowChange first{"t", "a", false, "val", "A"};
  UndoRowChange second{"t", "b", false, "val", "B"};

  pqxx::work  txn(*pg);
  UndoService svc(txn, *pg, schema);
  //
  //
  std::vector<UndoRowChange> undo = svc.collectUndoChanges({first, second});
  //
  //

  ASSERT_EQ(undo.size(), 2u);
  // Reverse order: the last user change is undone first.
  EXPECT_EQ(undo[0].idValue, "b");
  EXPECT_EQ(*undo[0].value, "2");
  EXPECT_EQ(undo[1].idValue, "a");
  EXPECT_EQ(*undo[1].value, "1");
}

TEST_F(UndoServiceTest, UpdateOnMissingRowIsUndoneByDelete)
{
  make_schema();
  setup_sql("CREATE TABLE " + qual("t") + " (id varchar(32) primary key, val text)");

  // Строки 'missing' ещё нет: применение изменения её создаст, поэтому отмена
  // должна удалить эту строку.
  UndoRowChange change{"t", "missing", false, "val", "x"};

  pqxx::work  txn(*pg);
  UndoService svc(txn, *pg, schema);
  //
  //
  std::vector<UndoRowChange> undo = svc.collectUndoChanges({change});
  //
  //

  ASSERT_EQ(undo.size(), 1u);
  EXPECT_EQ(undo[0].tableName, "t");
  EXPECT_EQ(undo[0].idValue, "missing");
  EXPECT_TRUE(undo[0].toDelete);
}

TEST_F(UndoServiceTest, DeleteIsNotSupportedAndThrows)
{
  make_schema();
  setup_sql("CREATE TABLE " + qual("t") + " (id varchar(32) primary key, val text)");
  setup_sql("INSERT INTO " + qual("t") + " (id, val) VALUES ('r1', 'old')");

  UndoRowChange change{"t", "r1", true, "", std::nullopt};

  pqxx::work  txn(*pg);
  UndoService svc(txn, *pg, schema);
  //
  //
  EXPECT_THROW(svc.collectUndoChanges({change}), std::exception);
  //
  //
}

TEST_F(UndoServiceTest, UpdatesAcrossThreeColumnsCaptureEachOldValue)
{
  make_schema();
  setup_sql("CREATE TABLE " + qual("t") + " (id varchar(32) primary key, a text, b int4, c bool)");
  setup_sql("INSERT INTO " + qual("t") + " (id, a, b, c) VALUES ('r1', 'old_a', 7, true)");

  // Three changes to the same row, one per non-id column.
  UndoRowChange ca{"t", "r1", false, "a", "new_a"};
  UndoRowChange cb{"t", "r1", false, "b", "42"};
  UndoRowChange cc{"t", "r1", false, "c", "false"};

  pqxx::work  txn(*pg);
  UndoService svc(txn, *pg, schema);
  //
  //
  std::vector<UndoRowChange> undo = svc.collectUndoChanges({ca, cb, cc});
  //
  //

  // Reverse order of the input; each restores its column's old value (read as text).
  ASSERT_EQ(undo.size(), 3u);

  EXPECT_EQ(undo[0].colName, "c");
  ASSERT_TRUE(undo[0].value.has_value());
  EXPECT_EQ(*undo[0].value, "t"); // PostgreSQL bool text form; 't'::bool round-trips back

  EXPECT_EQ(undo[1].colName, "b");
  ASSERT_TRUE(undo[1].value.has_value());
  EXPECT_EQ(*undo[1].value, "7");

  EXPECT_EQ(undo[2].colName, "a");
  ASSERT_TRUE(undo[2].value.has_value());
  EXPECT_EQ(*undo[2].value, "old_a");

  for (const UndoRowChange &u : undo) {
    EXPECT_EQ(u.tableName, "t");
    EXPECT_EQ(u.idValue, "r1");
    EXPECT_FALSE(u.toDelete);
  }
}

TEST_F(UndoServiceTest, UpdateUndoCapturesOldTimestampWithSubsecondPrecision)
{
  make_schema();
  // PostgreSQL timestamp keeps microsecond precision (6 fractional digits) — its
  // maximum; nanoseconds aren't representable in this type.
  const std::string original = "2026-06-18 12:34:56.123456";
  setup_sql("CREATE TABLE " + qual("t") + " (id varchar(32) primary key, ts timestamp)");
  setup_sql("INSERT INTO " + qual("t") + " (id, ts) VALUES ('r1', '" + original + "')");

  UndoRowChange change{"t", "r1", false, "ts", "2030-01-01 00:00:00"};

  pqxx::work  txn(*pg);
  UndoService svc(txn, *pg, schema);
  //
  //
  std::vector<UndoRowChange> undo = svc.collectUndoChanges({change});
  //
  //

  ASSERT_EQ(undo.size(), 1u);
  EXPECT_EQ(undo[0].colName, "ts");
  EXPECT_FALSE(undo[0].toDelete);
  ASSERT_TRUE(undo[0].value.has_value());
  // Full microsecond precision is captured (no milliseconds lost).
  EXPECT_EQ(*undo[0].value, original);

  // Round-trip: applying the captured value restores the exact same instant.
  txn.exec_params("UPDATE " + qual("t") + " SET ts = '2030-01-01 00:00:00' WHERE id = 'r1'");
  txn.exec_params("UPDATE " + qual("t") + " SET ts = $1 WHERE id = 'r1'", *undo[0].value);
  const std::string restored = txn.exec("SELECT ts FROM " + qual("t") + " WHERE id = 'r1'")[0][0].c_str();
  EXPECT_EQ(restored, original);
}
