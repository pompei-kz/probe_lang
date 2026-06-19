#include "back/DbTestBase.h"
#include "back/service/BlockServiceR.h"
#include "back/service/BlockServiceRW.h"
#include "back/service/ExprServiceR.h"
#include "back/service/ExprServiceRW.h"
#include "back/service/UnitServiceR.h"
#include "back/service/UnitServiceRW.h"
#include <gtest/gtest.h>

using namespace back;
using namespace back::model;

class ExprServiceRWTest : public DbTest
{
protected:
  Block reload_field()
  {
    auto [blocks, err] = load_blocks_in_view(conn(), schema, "u1", -1000, -1000, 1000, 1000);
    EXPECT_TRUE(err.empty()) << err;
    EXPECT_EQ(blocks.size(), 1u);
    return blocks.empty() ? Block{} : blocks[0];
  }
};

TEST_F(ExprServiceRWTest, SetThisTypeCreatesExpressionAndPersists)
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

TEST_F(ExprServiceRWTest, SetThisTypeTwiceUpdatesInPlace)
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

TEST_F(ExprServiceRWTest, SetUnitResolvesToExistingUnitName)
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

TEST_F(ExprServiceRWTest, DanglingUnitReferenceIsTreatedAsAbsent)
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

TEST_F(ExprServiceRWTest, ExprRectSyncsWorldCoordsAndSpatialQuery)
{
  make_schema();
  auto [fid, fmsg] = create_block(conn(), schema, "u1", BlockType::Field, 100, 200, 50, 20, "f");
  ASSERT_FALSE(fid.empty()) << fmsg;
  ASSERT_TRUE(update_field_expr_id_used(conn(), schema, fid, true).first);
  ASSERT_TRUE(set_field_expr_this_type(conn(), schema, fid, ExprType::ThisUnit).first);

  //
  //
  auto rect = update_field_expr_rect(conn(), schema, fid, 10.f, 2.f, 30.f, 12.f);
  //
  //

  ASSERT_TRUE(rect.first) << rect.second;

  // World rect = block.{x,y}(100,200) + slot(10,2), size 30x12 → (110,202,30,12).
  auto [in_view, e1] = load_exprs_in_view(conn(), schema, "u1", 0, 0, 1000, 1000);
  ASSERT_TRUE(e1.empty()) << e1;
  ASSERT_EQ(in_view.size(), 1u);
  EXPECT_FLOAT_EQ(in_view[0].x, 110.f);
  EXPECT_FLOAT_EQ(in_view[0].y, 202.f);
  EXPECT_FLOAT_EQ(in_view[0].width, 30.f);
  EXPECT_EQ(in_view[0].type, ExprType::ThisUnit);

  // A viewport far from the expression returns nothing.
  auto [off_view, e2] = load_exprs_in_view(conn(), schema, "u1", -1000, -1000, -500, -500);
  ASSERT_TRUE(e2.empty()) << e2;
  EXPECT_TRUE(off_view.empty());
}
