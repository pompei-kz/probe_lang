#include "DbTestBase.h"
#include "back/InitDb.h"
#include "back/UnitService.h"
#include <gtest/gtest.h>

#include <string>
#include <vector>

using namespace back;
using model::Unit;
using model::UnitType;

class UnitServiceTest : public DbTest
{
protected:
  std::vector<Unit> load_units()
  {
    pqxx::work txn(*pg);
    auto       u = load_units_for_schema(txn, *pg, schema);
    txn.commit();
    return u;
  }
};

// ---------------------------------------------------------------------------
// create_unit
// ---------------------------------------------------------------------------

TEST_F(UnitServiceTest, CreateUnitCreatesTableIfMissing)
{
  make_schema();
  ASSERT_FALSE(table_exists("unit"));

  //
  //
  auto [ok, msg] = create_unit(conn(), schema, "", "MyClass", UnitType::Class);
  //
  //

  ASSERT_TRUE(ok) << msg;
  EXPECT_TRUE(table_exists("unit"));
}

TEST_F(UnitServiceTest, CreateRootUnit)
{
  make_schema();

  //
  //
  auto [ok, msg] = create_unit(conn(), schema, "", "Root", UnitType::Class);
  //
  //

  ASSERT_TRUE(ok) << msg;
  auto units = load_units();
  ASSERT_EQ(units.size(), 1u);
  EXPECT_EQ(units[0].name, "Root");
  EXPECT_TRUE(units[0].parent_folder_id.empty());
  EXPECT_EQ(units[0].type, UnitType::Class);
}

TEST_F(UnitServiceTest, CreateUnitUnderFolder)
{
  make_schema();

  //
  //
  auto [ok, msg] = create_unit(conn(), schema, "folder123", "Child", UnitType::Interface);
  //
  //

  ASSERT_TRUE(ok) << msg;
  auto units = load_units();
  ASSERT_EQ(units.size(), 1u);
  EXPECT_EQ(units[0].parent_folder_id, "folder123");
  EXPECT_EQ(units[0].type, UnitType::Interface);
}

TEST_F(UnitServiceTest, CreateUnitStoresEnumType)
{
  make_schema();

  //
  //
  auto [ok, msg] = create_unit(conn(), schema, "", "Color", UnitType::Enum);
  //
  //

  ASSERT_TRUE(ok) << msg;
  auto units = load_units();
  ASSERT_EQ(units.size(), 1u);
  EXPECT_EQ(units[0].type, UnitType::Enum);
}

// ---------------------------------------------------------------------------
// edit_unit
// ---------------------------------------------------------------------------

TEST_F(UnitServiceTest, EditUnitChangesNameAndType)
{
  make_schema();
  ASSERT_TRUE(create_unit(conn(), schema, "", "Before", UnitType::Class).first);
  const std::string id = load_units().at(0).id;

  //
  //
  auto [ok, msg] = edit_unit(conn(), schema, id, "After", UnitType::Enum);
  //
  //

  ASSERT_TRUE(ok) << msg;
  auto units = load_units();
  ASSERT_EQ(units.size(), 1u);
  EXPECT_EQ(units[0].name, "After");
  EXPECT_EQ(units[0].type, UnitType::Enum);
}

// ---------------------------------------------------------------------------
// delete_unit
// ---------------------------------------------------------------------------

TEST_F(UnitServiceTest, DeleteUnitRemovesIt)
{
  make_schema();
  ASSERT_TRUE(create_unit(conn(), schema, "", "Doomed", UnitType::Class).first);
  ASSERT_TRUE(create_unit(conn(), schema, "", "Survivor", UnitType::Class).first);
  const std::string id = load_units().at(0).id; // "Doomed" sorts before "Survivor"

  //
  //
  auto [ok, msg] = delete_unit(conn(), schema, id);
  //
  //

  ASSERT_TRUE(ok) << msg;
  auto units = load_units();
  ASSERT_EQ(units.size(), 1u);
  EXPECT_EQ(units[0].name, "Survivor");
}

// ---------------------------------------------------------------------------
// load_units_for_schema
// ---------------------------------------------------------------------------

TEST_F(UnitServiceTest, LoadUnitsWithoutTableReturnsEmpty)
{
  make_schema(); // schema exists, but no unit table

  //
  //
  const std::vector<Unit> units = load_units();
  //
  //

  EXPECT_TRUE(units.empty());
}

TEST_F(UnitServiceTest, LoadUnitsSortedByName)
{
  make_schema();
  ASSERT_TRUE(create_unit(conn(), schema, "", "charlie", UnitType::Class).first);
  ASSERT_TRUE(create_unit(conn(), schema, "", "alpha", UnitType::Class).first);
  ASSERT_TRUE(create_unit(conn(), schema, "", "bravo", UnitType::Class).first);

  //
  //
  const std::vector<Unit> units = load_units();
  //
  //

  ASSERT_EQ(units.size(), 3u);
  EXPECT_EQ(units[0].name, "alpha");
  EXPECT_EQ(units[1].name, "bravo");
  EXPECT_EQ(units[2].name, "charlie");
}

// ---------------------------------------------------------------------------
// ensure_unit_tables
// ---------------------------------------------------------------------------

TEST_F(UnitServiceTest, EnsureUnitTablesCreatesMissingTable)
{
  // Simulate a repo schema created before the unit feature: it has
  // lang_setting but no unit table.
  make_schema();
  {
    pqxx::work txn(*pg);
    txn.exec("CREATE TABLE " + qual("lang_setting") + " (name varchar(150) PRIMARY KEY, value text)");
    txn.commit();
  }
  ASSERT_FALSE(table_exists("unit"));

  //
  //
  auto [ok, msg] = ensure_unit_tables(conn());
  //
  //

  ASSERT_TRUE(ok) << msg;
  EXPECT_TRUE(table_exists("unit"));
}

// ---------------------------------------------------------------------------
// list_units_paginated
// ---------------------------------------------------------------------------

TEST_F(UnitServiceTest, ListUnitsPaginatedFiltersByWordsInOrder)
{
  make_schema();
  ASSERT_TRUE(create_unit(conn(), schema, "", "Foo Bar", UnitType::Class).first);
  ASSERT_TRUE(create_unit(conn(), schema, "", "Foo Baz Bar", UnitType::Class).first);
  ASSERT_TRUE(create_unit(conn(), schema, "", "Bar Foo", UnitType::Class).first);

  //
  //
  auto [units, err] = list_units_paginated(conn(), schema, "foo bar", 0, 10);
  //
  //

  ASSERT_TRUE(err.empty()) << err;
  ASSERT_EQ(units.size(), 2u); // "Bar Foo" excluded — words out of order
  EXPECT_EQ(units[0].name, "Foo Bar");
  EXPECT_EQ(units[1].name, "Foo Baz Bar");
}

TEST_F(UnitServiceTest, ListUnitsPaginatedRespectsOffsetAndLimit)
{
  make_schema();
  ASSERT_TRUE(create_unit(conn(), schema, "", "u1", UnitType::Class).first);
  ASSERT_TRUE(create_unit(conn(), schema, "", "u2", UnitType::Class).first);
  ASSERT_TRUE(create_unit(conn(), schema, "", "u3", UnitType::Class).first);

  //
  //
  auto [page1, e1] = list_units_paginated(conn(), schema, "", 0, 2);
  auto [page2, e2] = list_units_paginated(conn(), schema, "", 2, 2);
  //
  //

  ASSERT_TRUE(e1.empty()) << e1;
  ASSERT_TRUE(e2.empty()) << e2;
  ASSERT_EQ(page1.size(), 2u);
  EXPECT_EQ(page1[0].name, "u1");
  EXPECT_EQ(page1[1].name, "u2");
  ASSERT_EQ(page2.size(), 1u); // last page is short — no more data
  EXPECT_EQ(page2[0].name, "u3");
}

TEST_F(UnitServiceTest, ListUnitsPaginatedEmptyFilterMatchesAll)
{
  make_schema();
  ASSERT_TRUE(create_unit(conn(), schema, "", "only", UnitType::Class).first);

  //
  //
  auto [units, err] = list_units_paginated(conn(), schema, "", 0, 10);
  //
  //

  ASSERT_TRUE(err.empty()) << err;
  EXPECT_EQ(units.size(), 1u);
}
