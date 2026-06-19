#include "back/DbTestBase.h"
#include "back/etc/InitDb.h"
#include "back/service/UnitServiceR.h"
#include "back/service/UnitServiceRW.h"
#include <gtest/gtest.h>

#include <string>
#include <vector>

class UnitServiceRWTest : public DbTest
{
protected:
  std::vector<back::model::Unit> load_units()
  {
    pqxx::work txn(*pg);
    auto       u = back::load_units_for_schema(txn, *pg, schema);
    txn.commit();
    return u;
  }
};

// ---------------------------------------------------------------------------
// create_unit
// ---------------------------------------------------------------------------

TEST_F(UnitServiceRWTest, CreateUnitCreatesTableIfMissing)
{
  make_schema();
  ASSERT_FALSE(table_exists("unit"));

  //
  //
  auto [ok, msg] = back::create_unit(conn(), schema, "", "MyClass", back::model::UnitType::Class);
  //
  //

  ASSERT_TRUE(ok) << msg;
  EXPECT_TRUE(table_exists("unit"));
}

TEST_F(UnitServiceRWTest, CreateRootUnit)
{
  make_schema();

  //
  //
  auto [ok, msg] = back::create_unit(conn(), schema, "", "Root", back::model::UnitType::Class);
  //
  //

  ASSERT_TRUE(ok) << msg;
  auto units = load_units();
  ASSERT_EQ(units.size(), 1u);
  EXPECT_EQ(units[0].name, "Root");
  EXPECT_TRUE(units[0].parent_folder_id.empty());
  EXPECT_EQ(units[0].type, back::model::UnitType::Class);
}

TEST_F(UnitServiceRWTest, CreateUnitUnderFolder)
{
  make_schema();

  //
  //
  auto [ok, msg] = back::create_unit(conn(), schema, "folder123", "Child", back::model::UnitType::Interface);
  //
  //

  ASSERT_TRUE(ok) << msg;
  auto units = load_units();
  ASSERT_EQ(units.size(), 1u);
  EXPECT_EQ(units[0].parent_folder_id, "folder123");
  EXPECT_EQ(units[0].type, back::model::UnitType::Interface);
}

TEST_F(UnitServiceRWTest, CreateUnitStoresEnumType)
{
  make_schema();

  //
  //
  auto [ok, msg] = back::create_unit(conn(), schema, "", "Color", back::model::UnitType::Enum);
  //
  //

  ASSERT_TRUE(ok) << msg;
  auto units = load_units();
  ASSERT_EQ(units.size(), 1u);
  EXPECT_EQ(units[0].type, back::model::UnitType::Enum);
}

// ---------------------------------------------------------------------------
// edit_unit
// ---------------------------------------------------------------------------

TEST_F(UnitServiceRWTest, EditUnitChangesNameAndType)
{
  make_schema();
  ASSERT_TRUE(back::create_unit(conn(), schema, "", "Before", back::model::UnitType::Class).first);
  const std::string id = load_units().at(0).id;

  //
  //
  auto [ok, msg] = back::edit_unit(conn(), schema, id, "After", back::model::UnitType::Enum);
  //
  //

  ASSERT_TRUE(ok) << msg;
  auto units = load_units();
  ASSERT_EQ(units.size(), 1u);
  EXPECT_EQ(units[0].name, "After");
  EXPECT_EQ(units[0].type, back::model::UnitType::Enum);
}

// ---------------------------------------------------------------------------
// delete_unit
// ---------------------------------------------------------------------------

TEST_F(UnitServiceRWTest, DeleteUnitRemovesIt)
{
  make_schema();
  ASSERT_TRUE(back::create_unit(conn(), schema, "", "Doomed", back::model::UnitType::Class).first);
  ASSERT_TRUE(back::create_unit(conn(), schema, "", "Survivor", back::model::UnitType::Class).first);
  const std::string id = load_units().at(0).id; // "Doomed" sorts before "Survivor"

  //
  //
  auto [ok, msg] = back::delete_unit(conn(), schema, id);
  //
  //

  ASSERT_TRUE(ok) << msg;
  auto units = load_units();
  ASSERT_EQ(units.size(), 1u);
  EXPECT_EQ(units[0].name, "Survivor");
}

// ---------------------------------------------------------------------------
// ensure_unit_tables
// ---------------------------------------------------------------------------

TEST_F(UnitServiceRWTest, EnsureUnitTablesCreatesMissingTable)
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
  auto [ok, msg] = back::ensure_unit_tables(conn());
  //
  //

  ASSERT_TRUE(ok) << msg;
  EXPECT_TRUE(table_exists("unit"));
}
