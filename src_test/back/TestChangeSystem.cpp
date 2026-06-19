#include "DbTestBase.h"
#include "back/ChangeSystem.h"
#include "back/InitDb.h"
#include <gtest/gtest.h>
#include <map>

using namespace back;
using namespace back::model;

class ChangeSystemTest : public DbTest
{
protected:
  // Run DDL/DML setup in its own committed transaction.
  void setup_sql(const std::string &sql)
  {
    pqxx::work txn(*pg);
    txn.exec(sql);
    txn.commit();
  }

  // Create the change-system tables (undo_buffer / undo_op / undo_row_change).
  void make_change_system()
  {
    pqxx::work txn(*pg);
    InitDb(txn, *pg, schema).init_change_system_tables();
    txn.commit();
  }

  // Apply a set of changes in its own committed transaction (test setup helper).
  void apply_committed(std::vector<RowChange> changes, const ChangeOp &op, const ChangeSysTarget &target)
  {
    pqxx::work txn(*pg);
    ChangeSystem(txn, *pg, schema).apply(std::move(changes), op, target);
    txn.commit();
  }
};

TEST_F(ChangeSystemTest, UpdateUndoCapturesOldColumnValue)
{
  make_schema();
  setup_sql("CREATE TABLE " + qual("t") + " (id varchar(32) primary key, val text)");
  setup_sql("INSERT INTO " + qual("t") + " (id, val) VALUES ('r1', 'old')");

  RowChange change{"t", "r1", false, "val", "new"};

  pqxx::work   txn(*pg);
  ChangeSystem svc(txn, *pg, schema);
  //
  //
  std::vector<RowChange> undo = svc.collectUndoChanges({change});
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

TEST_F(ChangeSystemTest, UpdateUndoOfNullOldValueIsNullopt)
{
  make_schema();
  setup_sql("CREATE TABLE " + qual("t") + " (id varchar(32) primary key, val text)");
  setup_sql("INSERT INTO " + qual("t") + " (id, val) VALUES ('r2', NULL)");

  RowChange change{"t", "r2", false, "val", "x"};

  pqxx::work   txn(*pg);
  ChangeSystem svc(txn, *pg, schema);
  //
  //
  std::vector<RowChange> undo = svc.collectUndoChanges({change});
  //
  //

  ASSERT_EQ(undo.size(), 1u);
  EXPECT_FALSE(undo[0].value.has_value()); // restore back to NULL
}

TEST_F(ChangeSystemTest, ChangesAreUndoneInReverseOrder)
{
  make_schema();
  setup_sql("CREATE TABLE " + qual("t") + " (id varchar(32) primary key, val text)");
  setup_sql("INSERT INTO " + qual("t") + " (id, val) VALUES ('a', '1'), ('b', '2')");

  RowChange first{"t", "a", false, "val", "A"};
  RowChange second{"t", "b", false, "val", "B"};

  pqxx::work   txn(*pg);
  ChangeSystem svc(txn, *pg, schema);
  //
  //
  std::vector<RowChange> undo = svc.collectUndoChanges({first, second});
  //
  //

  ASSERT_EQ(undo.size(), 2u);
  // Reverse order: the last user change is undone first.
  EXPECT_EQ(undo[0].idValue, "b");
  EXPECT_EQ(*undo[0].value, "2");
  EXPECT_EQ(undo[1].idValue, "a");
  EXPECT_EQ(*undo[1].value, "1");
}

TEST_F(ChangeSystemTest, UpdateOnMissingRowIsUndoneByDelete)
{
  make_schema();
  setup_sql("CREATE TABLE " + qual("t") + " (id varchar(32) primary key, val text)");

  // Строки 'missing' ещё нет: применение изменения её создаст, поэтому отмена
  // должна удалить эту строку.
  RowChange change{"t", "missing", false, "val", "x"};

  pqxx::work   txn(*pg);
  ChangeSystem svc(txn, *pg, schema);
  //
  //
  std::vector<RowChange> undo = svc.collectUndoChanges({change});
  //
  //

  ASSERT_EQ(undo.size(), 1u);
  EXPECT_EQ(undo[0].tableName, "t");
  EXPECT_EQ(undo[0].idValue, "missing");
  EXPECT_TRUE(undo[0].toDelete);
}

TEST_F(ChangeSystemTest, DeleteIsUndoneByPerColumnChanges)
{
  make_schema();
  setup_sql("CREATE TABLE " + qual("t") + " (id varchar(32) primary key, a text, b int4)");
  setup_sql("INSERT INTO " + qual("t") + " (id, a, b) VALUES ('r1', 'hello', 5)");

  RowChange change{"t", "r1", true, "", std::nullopt};

  pqxx::work   txn(*pg);
  ChangeSystem svc(txn, *pg, schema);

  //
  //
  std::vector<RowChange> undo = svc.collectUndoChanges({change});
  //
  //

  // One set-column change per non-id column of the deleted row, recreating it.
  ASSERT_EQ(undo.size(), 2u);
  std::map<std::string, std::optional<std::string>> byCol;
  for (const RowChange &u : undo) {
    EXPECT_EQ(u.tableName, "t");
    EXPECT_EQ(u.idValue, "r1");
    EXPECT_FALSE(u.toDelete);
    byCol[u.colName] = u.value;
  }
  EXPECT_FALSE(byCol.count("id")); // id передаётся через idValue, отдельного change нет
  ASSERT_TRUE(byCol.count("a") && byCol["a"].has_value());
  EXPECT_EQ(*byCol["a"], "hello");
  ASSERT_TRUE(byCol.count("b") && byCol["b"].has_value());
  EXPECT_EQ(*byCol["b"], "5");
}

TEST_F(ChangeSystemTest, DeleteOfMissingRowGivesNoUndo)
{
  make_schema();
  setup_sql("CREATE TABLE " + qual("t") + " (id varchar(32) primary key, val text)");

  RowChange change{"t", "missing", true, "", std::nullopt};

  pqxx::work   txn(*pg);
  ChangeSystem svc(txn, *pg, schema);
  //
  //
  std::vector<RowChange> undo = svc.collectUndoChanges({change});
  //
  //

  EXPECT_TRUE(undo.empty());
}

TEST_F(ChangeSystemTest, UpdatesAcrossThreeColumnsCaptureEachOldValue)
{
  make_schema();
  setup_sql("CREATE TABLE " + qual("t") + " (id varchar(32) primary key, a text, b int4, c bool)");
  setup_sql("INSERT INTO " + qual("t") + " (id, a, b, c) VALUES ('r1', 'old_a', 7, true)");

  // Three changes to the same row, one per non-id column.
  RowChange ca{"t", "r1", false, "a", "new_a"};
  RowChange cb{"t", "r1", false, "b", "42"};
  RowChange cc{"t", "r1", false, "c", "false"};

  pqxx::work   txn(*pg);
  ChangeSystem svc(txn, *pg, schema);
  //
  //
  std::vector<RowChange> undo = svc.collectUndoChanges({ca, cb, cc});
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

  for (const RowChange &u : undo) {
    EXPECT_EQ(u.tableName, "t");
    EXPECT_EQ(u.idValue, "r1");
    EXPECT_FALSE(u.toDelete);
  }
}

TEST_F(ChangeSystemTest, UpdateUndoCapturesOldTimestampWithSubsecondPrecision)
{
  make_schema();
  // PostgreSQL timestamp keeps microsecond precision (6 fractional digits) — its
  // maximum; nanoseconds aren't representable in this type.
  const std::string original = "2026-06-18 12:34:56.123456";
  setup_sql("CREATE TABLE " + qual("t") + " (id varchar(32) primary key, ts timestamp)");
  setup_sql("INSERT INTO " + qual("t") + " (id, ts) VALUES ('r1', '" + original + "')");

  RowChange change{"t", "r1", false, "ts", "2030-01-01 00:00:00"};

  pqxx::work   txn(*pg);
  ChangeSystem svc(txn, *pg, schema);
  //
  //
  std::vector<RowChange> undo = svc.collectUndoChanges({change});
  //
  //

  ASSERT_EQ(undo.size(), 1u);
  EXPECT_EQ(undo[0].colName, "ts");
  EXPECT_FALSE(undo[0].toDelete);
  ASSERT_TRUE(undo[0].value.has_value());
  // Full microsecond precision is captured (no milliseconds lost).
  EXPECT_EQ(*undo[0].value, original);

  // Round-trip: applying the captured value restores the exact same instant.
  txn.exec("UPDATE " + qual("t") + " SET ts = '2030-01-01 00:00:00' WHERE id = 'r1'");
  txn.exec("UPDATE " + qual("t") + " SET ts = $1 WHERE id = 'r1'", pqxx::params{txn, *undo[0].value});
  const std::string restored = txn.exec("SELECT ts FROM " + qual("t") + " WHERE id = 'r1'")[0][0].c_str();
  EXPECT_EQ(restored, original);
}

TEST_F(ChangeSystemTest, DeleteIsUndoneByThreeColumnChanges)
{
  make_schema();
  setup_sql("CREATE TABLE " + qual("t") + " (id varchar(32) primary key, a text, b int4, c bool)");
  setup_sql("INSERT INTO " + qual("t") + " (id, a, b, c) VALUES ('r1', 'hello', 5, true)");

  RowChange change{"t", "r1", true, "", std::nullopt};

  pqxx::work   txn(*pg);
  ChangeSystem svc(txn, *pg, schema);

  //
  //
  std::vector<RowChange> undo = svc.collectUndoChanges({change});
  //
  //

  // One set-column change per non-id column (three of them), recreating the row.
  ASSERT_EQ(undo.size(), 3u);
  std::map<std::string, std::optional<std::string>> byCol;
  for (const RowChange &u : undo) {
    EXPECT_EQ(u.tableName, "t");
    EXPECT_EQ(u.idValue, "r1");
    EXPECT_FALSE(u.toDelete);
    byCol[u.colName] = u.value;
  }
  EXPECT_FALSE(byCol.count("id"));
  ASSERT_TRUE(byCol.contains("a") && byCol["a"].has_value());
  EXPECT_EQ(*byCol["a"], "hello");
  ASSERT_TRUE(byCol.count("b") && byCol["b"].has_value());
  EXPECT_EQ(*byCol["b"], "5");
  ASSERT_TRUE(byCol.count("c") && byCol["c"].has_value());
  EXPECT_EQ(*byCol["c"], "t"); // PostgreSQL bool text form
}

// ---------------------------------------------------------------------------
// ChangeSystem::apply
// ---------------------------------------------------------------------------

TEST_F(ChangeSystemTest, ApplyUpdatesDataRow)
{
  make_schema();
  make_change_system();
  setup_sql("CREATE TABLE " + qual("t") + " (id varchar(32) primary key, val text)");
  setup_sql("INSERT INTO " + qual("t") + " (id, val) VALUES ('r1', 'old')");

  pqxx::work   txn(*pg);
  ChangeSystem svc(txn, *pg, schema);

  // std::vector userChanges {RowChange{"t", "r1", false, "val", "new"}};

  //
  //
  svc.apply({RowChange{"t", "r1", false, "val", "new"}}, ChangeOp{"set-val", "g1"}, ChangeSysTarget{"u1", "Unit"});
  //
  //

  const std::string val = txn.exec("SELECT val FROM " + qual("t") + " WHERE id = 'r1'")[0][0].c_str();
  EXPECT_EQ(val, "new");
}

TEST_F(ChangeSystemTest, ApplyCreatesUndoBufferForTarget)
{
  make_schema();
  make_change_system();
  setup_sql("CREATE TABLE " + qual("t") + " (id varchar(32) primary key, val text)");
  setup_sql("INSERT INTO " + qual("t") + " (id, val) VALUES ('r1', 'old')");

  pqxx::work   txn(*pg);
  ChangeSystem svc(txn, *pg, schema);
  //
  //
  svc.apply({RowChange{"t", "r1", false, "val", "new"}}, ChangeOp{"set-val", "g1"}, ChangeSysTarget{"u1", "Unit"});
  //
  //

  pqxx::row buf = txn.exec("SELECT target_id, target_type FROM " + qual("undo_buffer")).one_row();
  EXPECT_EQ(buf[0].c_str(), std::string("u1"));
  EXPECT_EQ(buf[1].c_str(), std::string("Unit"));
}

TEST_F(ChangeSystemTest, ApplyReusesBufferAndAppendsOp)
{
  make_schema();
  make_change_system();
  setup_sql("CREATE TABLE " + qual("t") + " (id varchar(32) primary key, val text)");
  setup_sql("INSERT INTO " + qual("t") + " (id, val) VALUES ('r1', 'a')");

  // First operation for the target — its buffer must be reused by the second.
  apply_committed({RowChange{"t", "r1", false, "val", "b"}}, ChangeOp{"op1", "g"}, ChangeSysTarget{"u1", "Unit"});

  pqxx::work   txn(*pg);
  ChangeSystem svc(txn, *pg, schema);
  //
  //
  svc.apply({RowChange{"t", "r1", false, "val", "c"}}, ChangeOp{"op2", "g"}, ChangeSysTarget{"u1", "Unit"});
  //
  //

  const long buffers = txn.exec("SELECT count(*) FROM " + qual("undo_buffer"))[0][0].as<long>();
  const long ops     = txn.exec("SELECT count(*) FROM " + qual("undo_op"))[0][0].as<long>();
  EXPECT_EQ(buffers, 1); // same target → single buffer
  EXPECT_EQ(ops, 2);     // two operations appended to it
}

TEST_F(ChangeSystemTest, ApplyStoresOperationMetadata)
{
  make_schema();
  make_change_system();
  setup_sql("CREATE TABLE " + qual("t") + " (id varchar(32) primary key, val text)");
  setup_sql("INSERT INTO " + qual("t") + " (id, val) VALUES ('r1', 'old')");

  pqxx::work   txn(*pg);
  ChangeSystem svc(txn, *pg, schema);
  //
  //
  svc.apply({RowChange{"t", "r1", false, "val", "new"}}, ChangeOp{"rename", "edit-block"}, ChangeSysTarget{"u1", "Unit"});
  //
  //

  pqxx::row op = txn.exec("SELECT name, group_name, undone FROM " + qual("undo_op")).one_row();
  EXPECT_EQ(op[0].c_str(), std::string("rename"));
  EXPECT_EQ(op[1].c_str(), std::string("edit-block"));
  EXPECT_FALSE(op[2].as<bool>()); // a freshly applied op is not undone
}

TEST_F(ChangeSystemTest, ApplyStoresForwardAndForUndoChanges)
{
  make_schema();
  make_change_system();
  setup_sql("CREATE TABLE " + qual("t") + " (id varchar(32) primary key, val text)");
  setup_sql("INSERT INTO " + qual("t") + " (id, val) VALUES ('r1', 'old')");

  pqxx::work   txn(*pg);
  ChangeSystem svc(txn, *pg, schema);
  //
  //
  svc.apply({RowChange{"t", "r1", false, "val", "new"}}, ChangeOp{"set-val", "g1"}, ChangeSysTarget{"u1", "Unit"});
  //
  //

  // Forward = what the user did; ForUndo = what reverts it (the old value).
  pqxx::row fwd =
      txn.exec("SELECT table_name, id_value, to_delete, col_name, col_value FROM " + qual("undo_row_change") + " WHERE direction = 'Forward'")
          .one_row();
  EXPECT_EQ(fwd[0].c_str(), std::string("t"));
  EXPECT_EQ(fwd[1].c_str(), std::string("r1"));
  EXPECT_FALSE(fwd[2].as<bool>());
  EXPECT_EQ(fwd[3].c_str(), std::string("val"));
  EXPECT_EQ(fwd[4].c_str(), std::string("new"));

  pqxx::row undo = txn.exec("SELECT col_name, col_value FROM " + qual("undo_row_change") + " WHERE direction = 'ForUndo'").one_row();
  EXPECT_EQ(undo[0].c_str(), std::string("val"));
  EXPECT_EQ(undo[1].c_str(), std::string("old")); // captured before the update
}

TEST_F(ChangeSystemTest, ApplyMakesNewOpTheActiveOne)
{
  make_schema();
  make_change_system();
  setup_sql("CREATE TABLE " + qual("t") + " (id varchar(32) primary key, val text)");
  setup_sql("INSERT INTO " + qual("t") + " (id, val) VALUES ('r1', 'a')");

  // A first operation already exists for the target.
  apply_committed({RowChange{"t", "r1", false, "val", "b"}}, ChangeOp{"op1", "g"}, ChangeSysTarget{"u1", "Unit"});

  pqxx::work   txn(*pg);
  ChangeSystem svc(txn, *pg, schema);
  //
  //
  svc.apply({RowChange{"t", "r1", false, "val", "c"}}, ChangeOp{"op2", "g"}, ChangeSysTarget{"u1", "Unit"});
  //
  //

  // The active operation is the last one with undone = FALSE — i.e. the one just applied.
  const std::string active =
      txn.exec("SELECT name FROM " + qual("undo_op") + " WHERE undone = FALSE ORDER BY order_index DESC LIMIT 1")[0][0].c_str();
  EXPECT_EQ(active, "op2");
}

TEST_F(ChangeSystemTest, ApplyWipesRedo)
{
  make_schema();
  make_change_system();
  setup_sql("CREATE TABLE " + qual("t") + " (id varchar(32) primary key, val text)");
  setup_sql("INSERT INTO " + qual("t") + " (id, val) VALUES ('r1', 'a')");

  // A previously-undone operation (redo candidate) must be discarded by a new apply.
  apply_committed({RowChange{"t", "r1", false, "val", "b"}}, ChangeOp{"op1", "g"}, ChangeSysTarget{"u1", "Unit"});
  setup_sql("UPDATE " + qual("undo_op") + " SET undone = TRUE");

  pqxx::work   txn(*pg);
  ChangeSystem svc(txn, *pg, schema);
  //
  //
  svc.apply({RowChange{"t", "r1", false, "val", "c"}}, ChangeOp{"op2", "g"}, ChangeSysTarget{"u1", "Unit"});
  //
  //

  const long ops    = txn.exec("SELECT count(*) FROM " + qual("undo_op"))[0][0].as<long>();
  const long undone = txn.exec("SELECT count(*) FROM " + qual("undo_op") + " WHERE undone = TRUE")[0][0].as<long>();
  const long orphan = txn.exec("SELECT count(*) FROM " + qual("undo_row_change") +
                               " rc "
                               "LEFT JOIN " +
                               qual("undo_op") + " op ON op.id = rc.undo_op_id WHERE op.id IS NULL")[0][0]
                          .as<long>();
  EXPECT_EQ(ops, 1);    // only the newly applied op remains
  EXPECT_EQ(undone, 0); // the redo candidate is gone
  EXPECT_EQ(orphan, 0); // its row changes were removed too
}

TEST_F(ChangeSystemTest, ApplyCreatesRowViaUpsert)
{
  make_schema();
  make_change_system();
  setup_sql("CREATE TABLE " + qual("t") + " (id varchar(32) primary key, val text)");

  pqxx::work   txn(*pg);
  ChangeSystem svc(txn, *pg, schema);
  //
  //
  svc.apply({RowChange{"t", "r2", false, "val", "created"}}, ChangeOp{"create", "g"}, ChangeSysTarget{"u1", "Unit"});
  //
  //

  // The row is created in the data table...
  pqxx::result row = txn.exec("SELECT val FROM " + qual("t") + " WHERE id = 'r2'");
  ASSERT_EQ(row.size(), 1u);
  EXPECT_EQ(row[0][0].c_str(), std::string("created"));

  // ...and undoing a creation is a deletion.
  pqxx::row undo = txn.exec("SELECT to_delete FROM " + qual("undo_row_change") + " WHERE direction = 'ForUndo'").one_row();
  EXPECT_TRUE(undo[0].as<bool>());
}

TEST_F(ChangeSystemTest, ApplyDeletesRow)
{
  make_schema();
  make_change_system();
  setup_sql("CREATE TABLE " + qual("t") + " (id varchar(32) primary key, a text, b int4)");
  setup_sql("INSERT INTO " + qual("t") + " (id, a, b) VALUES ('r1', 'hello', 5)");

  pqxx::work   txn(*pg);
  ChangeSystem svc(txn, *pg, schema);
  //
  //
  svc.apply({RowChange{"t", "r1", true, "", std::nullopt}}, ChangeOp{"delete", "g"}, ChangeSysTarget{"u1", "Unit"});
  //
  //

  // The row is removed from the data table...
  EXPECT_TRUE(txn.exec("SELECT * FROM " + qual("t") + " WHERE id = 'r1'").empty());

  // ...and the deletion is undone by one set-column change per non-id column.
  const long forUndo =
      txn.exec("SELECT count(*) FROM " + qual("undo_row_change") + " WHERE direction = 'ForUndo' AND to_delete = FALSE")[0][0].as<long>();
  EXPECT_EQ(forUndo, 2); // columns a and b
}
