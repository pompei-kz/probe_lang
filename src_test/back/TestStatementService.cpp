#include "DbTestBase.h"
#include "back/StatementService.h"
#include <gtest/gtest.h>

#include <string>
#include <vector>

using namespace back;
using namespace back::model;

class StatementServiceTest : public DbTest
{
protected:
  // Name stored in the detail table for a given unit_st id ("" if absent).
  std::string detail_name(const std::string &table, const std::string &id)
  {
    pqxx::work txn(*pg);
    auto       r = txn.exec_params("SELECT name FROM " + qual(table) + " WHERE id = $1", id);
    return r.empty() ? "" : r[0][0].c_str();
  }

  long unit_st_count()
  {
    pqxx::work txn(*pg);
    return txn.exec("SELECT count(*) FROM " + qual("unit_st"))[0][0].as<long>();
  }
};

// ---------------------------------------------------------------------------
// create_statement
// ---------------------------------------------------------------------------

TEST_F(StatementServiceTest, CreateStatementCreatesTablesIfMissing)
{
  make_schema();
  ASSERT_FALSE(table_exists("unit_st"));

  //
  //
  auto [id, msg] = create_statement(conn(), schema, "u1", StatementType::Method, 0, 0, 100, 40, "m");
  //
  //

  ASSERT_FALSE(id.empty()) << msg;
  EXPECT_TRUE(table_exists("unit_st"));
  EXPECT_TRUE(table_exists("unit_st_method"));
  EXPECT_TRUE(table_exists("unit_st_field"));
}

TEST_F(StatementServiceTest, CreateMethodInsertsUnitStAndMethodRow)
{
  make_schema();

  //
  //
  auto [id, msg] = create_statement(conn(), schema, "u1", StatementType::Method, 10, 20, 30, 40, "doWork");
  //
  //

  ASSERT_FALSE(id.empty()) << msg;
  EXPECT_EQ(unit_st_count(), 1);
  EXPECT_EQ(detail_name("unit_st_method", id), "doWork");
  EXPECT_EQ(detail_name("unit_st_field", id), ""); // not in the field table
}

TEST_F(StatementServiceTest, CreateFieldInsertsIntoFieldTable)
{
  make_schema();

  //
  //
  auto [id, msg] = create_statement(conn(), schema, "u1", StatementType::Field, 0, 0, 100, 40, "count");
  //
  //

  ASSERT_FALSE(id.empty()) << msg;
  EXPECT_EQ(detail_name("unit_st_field", id), "count");
  EXPECT_EQ(detail_name("unit_st_method", id), "");
}

// ---------------------------------------------------------------------------
// load_statements_in_view
// ---------------------------------------------------------------------------

TEST_F(StatementServiceTest, LoadReturnsStatementIntersectingView)
{
  make_schema();
  auto [id, msg] = create_statement(conn(), schema, "u1", StatementType::Method, 10, 20, 30, 40, "m");
  ASSERT_FALSE(id.empty()) << msg;

  //
  //
  auto [stmts, err] = load_statements_in_view(conn(), schema, "u1", 0, 0, 100, 100);
  //
  //

  ASSERT_TRUE(err.empty()) << err;
  ASSERT_EQ(stmts.size(), 1u);
  EXPECT_EQ(stmts[0].id, id);
  EXPECT_EQ(stmts[0].unit_id, "u1");
  EXPECT_EQ(stmts[0].type, StatementType::Method);
  EXPECT_EQ(stmts[0].name, "m");
  EXPECT_FLOAT_EQ(stmts[0].x, 10);
  EXPECT_FLOAT_EQ(stmts[0].y, 20);
  EXPECT_FLOAT_EQ(stmts[0].width, 30);
  EXPECT_FLOAT_EQ(stmts[0].height, 40);
}

TEST_F(StatementServiceTest, LoadExcludesStatementOutsideView)
{
  make_schema();
  auto [id, msg] = create_statement(conn(), schema, "u1", StatementType::Method, 10, 20, 30, 40, "m");
  ASSERT_FALSE(id.empty()) << msg;

  //
  //
  auto [stmts, err] = load_statements_in_view(conn(), schema, "u1", 500, 500, 600, 600);
  //
  //

  ASSERT_TRUE(err.empty()) << err;
  EXPECT_TRUE(stmts.empty());
}

TEST_F(StatementServiceTest, LoadExcludesStatementsOfOtherUnits)
{
  make_schema();
  ASSERT_FALSE(create_statement(conn(), schema, "u1", StatementType::Method, 0, 0, 50, 20, "mine").first.empty());
  ASSERT_FALSE(create_statement(conn(), schema, "u2", StatementType::Method, 0, 30, 50, 20, "other").first.empty());

  //
  //
  auto [stmts, err] = load_statements_in_view(conn(), schema, "u1", -1000, -1000, 1000, 1000);
  //
  //

  ASSERT_TRUE(err.empty()) << err;
  ASSERT_EQ(stmts.size(), 1u);
  EXPECT_EQ(stmts[0].name, "mine");
}

TEST_F(StatementServiceTest, LoadReturnsBothMethodAndFieldNames)
{
  make_schema();
  ASSERT_FALSE(create_statement(conn(), schema, "u1", StatementType::Method, 0, 0, 50, 20, "doWork").first.empty());
  ASSERT_FALSE(create_statement(conn(), schema, "u1", StatementType::Field, 0, 30, 50, 20, "count").first.empty());

  //
  //
  auto [stmts, err] = load_statements_in_view(conn(), schema, "u1", -1000, -1000, 1000, 1000);
  //
  //

  ASSERT_TRUE(err.empty()) << err;
  ASSERT_EQ(stmts.size(), 2u);
  bool sawMethod = false, sawField = false;
  for (const auto &s : stmts) {
    if (s.type == StatementType::Method) { sawMethod = (s.name == "doWork"); }
    if (s.type == StatementType::Field) { sawField = (s.name == "count"); }
  }
  EXPECT_TRUE(sawMethod);
  EXPECT_TRUE(sawField);
}

TEST_F(StatementServiceTest, LoadEmptyWhenTableMissing)
{
  make_schema(); // schema exists, but no unit_st table

  //
  //
  auto [stmts, err] = load_statements_in_view(conn(), schema, "u1", 0, 0, 100, 100);
  //
  //

  ASSERT_TRUE(err.empty()) << err;
  EXPECT_TRUE(stmts.empty());
}

// ---------------------------------------------------------------------------
// statement_bbox_for_unit
// ---------------------------------------------------------------------------

TEST_F(StatementServiceTest, BboxSpansAllStatementsOfUnit)
{
  make_schema();
  ASSERT_FALSE(create_statement(conn(), schema, "u1", StatementType::Method, 10, 10, 20, 20, "a").first.empty());
  ASSERT_FALSE(create_statement(conn(), schema, "u1", StatementType::Field, 50, 60, 20, 20, "b").first.empty());
  // Another unit's statement must not affect u1's bbox.
  ASSERT_FALSE(create_statement(conn(), schema, "u2", StatementType::Method, 500, 500, 20, 20, "c").first.empty());

  //
  //
  auto [box, err] = statement_bbox_for_unit(conn(), schema, "u1");
  //
  //

  ASSERT_TRUE(err.empty()) << err;
  ASSERT_TRUE(box.has_value());
  EXPECT_FLOAT_EQ(box->min_x, 10);
  EXPECT_FLOAT_EQ(box->min_y, 10);
  EXPECT_FLOAT_EQ(box->max_x, 70); // 50 + 20
  EXPECT_FLOAT_EQ(box->max_y, 80); // 60 + 20
}

TEST_F(StatementServiceTest, BboxNullWhenUnitHasNoStatements)
{
  make_schema();
  ASSERT_FALSE(create_statement(conn(), schema, "u1", StatementType::Method, 0, 0, 20, 20, "a").first.empty());

  //
  //
  auto [box, err] = statement_bbox_for_unit(conn(), schema, "other");
  //
  //

  ASSERT_TRUE(err.empty()) << err;
  EXPECT_FALSE(box.has_value());
}

// ---------------------------------------------------------------------------
// update_statement_name
// ---------------------------------------------------------------------------

TEST_F(StatementServiceTest, UpdateStatementNamePersists)
{
  make_schema();
  auto [id, msg] = create_statement(conn(), schema, "u1", StatementType::Method, 0, 0, 50, 20, "old");
  ASSERT_FALSE(id.empty()) << msg;

  //
  //
  auto [ok, err] = update_statement_name(conn(), schema, id, StatementType::Method, "renamed");
  //
  //

  ASSERT_TRUE(ok) << err;
  EXPECT_EQ(detail_name("unit_st_method", id), "renamed");
}

// ---------------------------------------------------------------------------
// update_statement_size
// ---------------------------------------------------------------------------

TEST_F(StatementServiceTest, UpdateStatementSizePersists)
{
  make_schema();
  auto [id, msg] = create_statement(conn(), schema, "u1", StatementType::Method, 0, 0, 50, 20, "m");
  ASSERT_FALSE(id.empty()) << msg;

  //
  //
  auto [ok, err] = update_statement_size(conn(), schema, id, 222, 33);
  //
  //

  ASSERT_TRUE(ok) << err;
  auto [stmts, lerr] = load_statements_in_view(conn(), schema, "u1", -1000, -1000, 1000, 1000);
  ASSERT_TRUE(lerr.empty()) << lerr;
  ASSERT_EQ(stmts.size(), 1u);
  EXPECT_FLOAT_EQ(stmts[0].width, 222);
  EXPECT_FLOAT_EQ(stmts[0].height, 33);
}
