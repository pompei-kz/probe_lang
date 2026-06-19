#include "back/DbTestBase.h"
#include "back/etc/InitDb.h"
#include "back/service/UnitServiceR.h"
#include "back/service/UnitServiceRW.h"
#include <gtest/gtest.h>

#include <string>
#include <vector>

using namespace back;
using model::Unit;
using model::UnitType;

class UnitServiceRTest : public DbTest
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
// load_units_for_schema
// ---------------------------------------------------------------------------

TEST_F(UnitServiceRTest, LoadUnitsWithoutTableReturnsEmpty)
{
  make_schema(); // schema exists, but no unit table

  //
  //
  const std::vector<Unit> units = load_units();
  //
  //

  EXPECT_TRUE(units.empty());
}

TEST_F(UnitServiceRTest, LoadUnitsSortedByName)
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
// list_units_paginated
// ---------------------------------------------------------------------------

TEST_F(UnitServiceRTest, ListUnitsPaginatedFiltersByWordsInOrder)
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

TEST_F(UnitServiceRTest, ListUnitsPaginatedRespectsOffsetAndLimit)
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

TEST_F(UnitServiceRTest, ListUnitsPaginatedEmptyFilterMatchesAll)
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
