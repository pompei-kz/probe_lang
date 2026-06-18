#include "DbTestBase.h"
#include "back/BlockService.h"
#include "back/ExprService.h"
#include "back/UnitService.h"
#include <gtest/gtest.h>

using namespace back;
using namespace back::model;

class ExprServiceTest : public DbTest
{
protected:
  // id of the only field block in the unit (assumes exactly one).
  std::string only_field()
  {
    auto [blocks, err] = load_blocks_in_view(conn(), schema, "u1", -1000, -1000, 1000, 1000);
    EXPECT_TRUE(err.empty()) << err;
    EXPECT_EQ(blocks.size(), 1u);
    return blocks.empty() ? "" : blocks[0].id;
  }

  Block reload_field()
  {
    auto [blocks, err] = load_blocks_in_view(conn(), schema, "u1", -1000, -1000, 1000, 1000);
    EXPECT_TRUE(err.empty()) << err;
    EXPECT_EQ(blocks.size(), 1u);
    return blocks.empty() ? Block{} : blocks[0];
  }
};

TEST_F(ExprServiceTest, SetThisTypeCreatesExpressionAndPersists)
{
  make_schema();
  auto [fid, fmsg] = create_block(conn(), schema, "u1", BlockType::Field, 0, 0, 50, 20, "f");
  ASSERT_FALSE(fid.empty()) << fmsg;

  //
  //
  auto set = set_field_expr_this_type(conn(), schema, fid, ExprType::ThisMethod);
  //
  //

  ASSERT_TRUE(set.first) << set.second;
  Block b = reload_field();
  EXPECT_TRUE(b.expr_present);
  EXPECT_EQ(b.expr_type, ExprType::ThisMethod);
}

TEST_F(ExprServiceTest, SetThisTypeTwiceUpdatesInPlace)
{
  make_schema();
  auto [fid, fmsg] = create_block(conn(), schema, "u1", BlockType::Field, 0, 0, 50, 20, "f");
  ASSERT_FALSE(fid.empty()) << fmsg;

  ASSERT_TRUE(set_field_expr_this_type(conn(), schema, fid, ExprType::ThisObject).first);
  //
  //
  auto set = set_field_expr_this_type(conn(), schema, fid, ExprType::ThisUnit);
  //
  //

  ASSERT_TRUE(set.first) << set.second;
  // Exactly one unit_e row — the type was updated, not duplicated.
  pqxx::work txn(*pg);
  EXPECT_EQ(txn.exec("SELECT count(*) FROM " + qual("unit_e"))[0][0].as<long>(), 1);
  EXPECT_EQ(reload_field().expr_type, ExprType::ThisUnit);
}

TEST_F(ExprServiceTest, SetUnitResolvesToExistingUnitName)
{
  make_schema();
  ASSERT_TRUE(create_unit(conn(), schema, "", "Альфа", UnitType::Class).first);
  auto [units, lerr] = list_units_paginated(conn(), schema, "Альфа", 0, 10);
  ASSERT_TRUE(lerr.empty()) << lerr;
  ASSERT_EQ(units.size(), 1u);

  auto [fid, fmsg] = create_block(conn(), schema, "u1", BlockType::Field, 0, 0, 50, 20, "f");
  ASSERT_FALSE(fid.empty()) << fmsg;

  //
  //
  auto set = set_field_expr_unit(conn(), schema, fid, units[0].id);
  //
  //

  ASSERT_TRUE(set.first) << set.second;
  Block b = reload_field();
  EXPECT_TRUE(b.expr_present);
  EXPECT_EQ(b.expr_type, ExprType::Unit);
  EXPECT_TRUE(b.expr_unit_present);
  EXPECT_EQ(b.expr_unit_name, "Альфа");
}

TEST_F(ExprServiceTest, DanglingUnitReferenceIsTreatedAsAbsent)
{
  make_schema();
  auto [fid, fmsg] = create_block(conn(), schema, "u1", BlockType::Field, 0, 0, 50, 20, "f");
  ASSERT_FALSE(fid.empty()) << fmsg;

  //
  //
  auto set = set_field_expr_unit(conn(), schema, fid, "no_such_unit_id");
  //
  //

  ASSERT_TRUE(set.first) << set.second;
  Block b = reload_field();
  // Soft coupling: the expression exists (Unit), but its unit reference dangles.
  EXPECT_TRUE(b.expr_present);
  EXPECT_EQ(b.expr_type, ExprType::Unit);
  EXPECT_FALSE(b.expr_unit_present);
  EXPECT_TRUE(b.expr_unit_name.empty());
}

TEST_F(ExprServiceTest, NoExpressionWhenNoneSet)
{
  make_schema();
  auto [fid, fmsg] = create_block(conn(), schema, "u1", BlockType::Field, 0, 0, 50, 20, "f");
  ASSERT_FALSE(fid.empty()) << fmsg;

  //
  //
  Block b = reload_field();
  //
  //

  EXPECT_FALSE(b.expr_present);
}
