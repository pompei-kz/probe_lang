#include "DbTestBase.h"
#include "back/BlockService.h"
#include <gtest/gtest.h>

#include <string>
#include <vector>

using namespace back;
using namespace back::model;

class BlockServiceTest : public DbTest
{
protected:
  // Name stored in the detail table for a given unit_bl id ("" if absent).
  std::string detail_name(const std::string &table, const std::string &id)
  {
    pqxx::work txn(*pg);
    auto       r = txn.exec_params("SELECT name FROM " + qual(table) + " WHERE id = $1", id);
    return r.empty() ? "" : r[0][0].c_str();
  }

  long unit_bl_count()
  {
    pqxx::work txn(*pg);
    return txn.exec("SELECT count(*) FROM " + qual("unit_bl"))[0][0].as<long>();
  }
};

// ---------------------------------------------------------------------------
// create_block
// ---------------------------------------------------------------------------

TEST_F(BlockServiceTest, CreateBlockCreatesTablesIfMissing)
{
  make_schema();
  ASSERT_FALSE(table_exists("unit_bl"));

  //
  //
  auto [id, msg] = create_block(conn(), schema, "u1", BlockType::Method, 0, 0, 100, 40, "m");
  //
  //

  ASSERT_FALSE(id.empty()) << msg;
  EXPECT_TRUE(table_exists("unit_bl"));
  EXPECT_TRUE(table_exists("unit_bl_method"));
  EXPECT_TRUE(table_exists("unit_bl_field"));
}

TEST_F(BlockServiceTest, CreateMethodInsertsUnitBlAndMethodRow)
{
  make_schema();

  //
  //
  auto [id, msg] = create_block(conn(), schema, "u1", BlockType::Method, 10, 20, 30, 40, "doWork");
  //
  //

  ASSERT_FALSE(id.empty()) << msg;
  EXPECT_EQ(unit_bl_count(), 1);
  EXPECT_EQ(detail_name("unit_bl_method", id), "doWork");
  EXPECT_EQ(detail_name("unit_bl_field", id), ""); // not in the field table
}

TEST_F(BlockServiceTest, CreateFieldInsertsIntoFieldTable)
{
  make_schema();

  //
  //
  auto [id, msg] = create_block(conn(), schema, "u1", BlockType::Field, 0, 0, 100, 40, "count");
  //
  //

  ASSERT_FALSE(id.empty()) << msg;
  EXPECT_EQ(detail_name("unit_bl_field", id), "count");
  EXPECT_EQ(detail_name("unit_bl_method", id), "");
}

// ---------------------------------------------------------------------------
// load_blocks_in_view
// ---------------------------------------------------------------------------

TEST_F(BlockServiceTest, LoadReturnsBlockIntersectingView)
{
  make_schema();
  auto [id, msg] = create_block(conn(), schema, "u1", BlockType::Method, 10, 20, 30, 40, "m");
  ASSERT_FALSE(id.empty()) << msg;

  //
  //
  auto [blocks, err] = load_blocks_in_view(conn(), schema, "u1", 0, 0, 100, 100);
  //
  //

  ASSERT_TRUE(err.empty()) << err;
  ASSERT_EQ(blocks.size(), 1u);
  EXPECT_EQ(blocks[0].id, id);
  EXPECT_EQ(blocks[0].unit_id, "u1");
  EXPECT_EQ(blocks[0].type, BlockType::Method);
  EXPECT_EQ(blocks[0].name, "m");
  EXPECT_FLOAT_EQ(blocks[0].x, 10);
  EXPECT_FLOAT_EQ(blocks[0].y, 20);
  EXPECT_FLOAT_EQ(blocks[0].width, 30);
  EXPECT_FLOAT_EQ(blocks[0].height, 40);
}

TEST_F(BlockServiceTest, LoadExcludesBlockOutsideView)
{
  make_schema();
  auto [id, msg] = create_block(conn(), schema, "u1", BlockType::Method, 10, 20, 30, 40, "m");
  ASSERT_FALSE(id.empty()) << msg;

  //
  //
  auto [blocks, err] = load_blocks_in_view(conn(), schema, "u1", 500, 500, 600, 600);
  //
  //

  ASSERT_TRUE(err.empty()) << err;
  EXPECT_TRUE(blocks.empty());
}

TEST_F(BlockServiceTest, LoadExcludesBlocksOfOtherUnits)
{
  make_schema();
  ASSERT_FALSE(create_block(conn(), schema, "u1", BlockType::Method, 0, 0, 50, 20, "mine").first.empty());
  ASSERT_FALSE(create_block(conn(), schema, "u2", BlockType::Method, 0, 30, 50, 20, "other").first.empty());

  //
  //
  auto [blocks, err] = load_blocks_in_view(conn(), schema, "u1", -1000, -1000, 1000, 1000);
  //
  //

  ASSERT_TRUE(err.empty()) << err;
  ASSERT_EQ(blocks.size(), 1u);
  EXPECT_EQ(blocks[0].name, "mine");
}

TEST_F(BlockServiceTest, LoadReturnsBothMethodAndFieldNames)
{
  make_schema();
  ASSERT_FALSE(create_block(conn(), schema, "u1", BlockType::Method, 0, 0, 50, 20, "doWork").first.empty());
  ASSERT_FALSE(create_block(conn(), schema, "u1", BlockType::Field, 0, 30, 50, 20, "count").first.empty());

  //
  //
  auto [blocks, err] = load_blocks_in_view(conn(), schema, "u1", -1000, -1000, 1000, 1000);
  //
  //

  ASSERT_TRUE(err.empty()) << err;
  ASSERT_EQ(blocks.size(), 2u);
  bool sawMethod = false, sawField = false;
  for (const auto &s : blocks) {
    if (s.type == BlockType::Method) { sawMethod = (s.name == "doWork"); }
    if (s.type == BlockType::Field) { sawField = (s.name == "count"); }
  }
  EXPECT_TRUE(sawMethod);
  EXPECT_TRUE(sawField);
}

TEST_F(BlockServiceTest, LoadEmptyWhenTableMissing)
{
  make_schema(); // schema exists, but no unit_bl table

  //
  //
  auto [blocks, err] = load_blocks_in_view(conn(), schema, "u1", 0, 0, 100, 100);
  //
  //

  ASSERT_TRUE(err.empty()) << err;
  EXPECT_TRUE(blocks.empty());
}

// ---------------------------------------------------------------------------
// block_bbox_for_unit
// ---------------------------------------------------------------------------

TEST_F(BlockServiceTest, BboxSpansAllBlocksOfUnit)
{
  make_schema();
  ASSERT_FALSE(create_block(conn(), schema, "u1", BlockType::Method, 10, 10, 20, 20, "a").first.empty());
  ASSERT_FALSE(create_block(conn(), schema, "u1", BlockType::Field, 50, 60, 20, 20, "b").first.empty());
  // Another unit's block must not affect u1's bbox.
  ASSERT_FALSE(create_block(conn(), schema, "u2", BlockType::Method, 500, 500, 20, 20, "c").first.empty());

  //
  //
  auto [box, err] = block_bbox_for_unit(conn(), schema, "u1");
  //
  //

  ASSERT_TRUE(err.empty()) << err;
  ASSERT_TRUE(box.has_value());
  EXPECT_FLOAT_EQ(box->min_x, 10);
  EXPECT_FLOAT_EQ(box->min_y, 10);
  EXPECT_FLOAT_EQ(box->max_x, 70); // 50 + 20
  EXPECT_FLOAT_EQ(box->max_y, 80); // 60 + 20
}

TEST_F(BlockServiceTest, BboxNullWhenUnitHasNoBlocks)
{
  make_schema();
  ASSERT_FALSE(create_block(conn(), schema, "u1", BlockType::Method, 0, 0, 20, 20, "a").first.empty());

  //
  //
  auto [box, err] = block_bbox_for_unit(conn(), schema, "other");
  //
  //

  ASSERT_TRUE(err.empty()) << err;
  EXPECT_FALSE(box.has_value());
}

// ---------------------------------------------------------------------------
// update_block_name
// ---------------------------------------------------------------------------

TEST_F(BlockServiceTest, UpdateBlockNamePersists)
{
  make_schema();
  auto [id, msg] = create_block(conn(), schema, "u1", BlockType::Method, 0, 0, 50, 20, "old");
  ASSERT_FALSE(id.empty()) << msg;

  //
  //
  auto [ok, err] = update_block_name(conn(), schema, id, BlockType::Method, "renamed");
  //
  //

  ASSERT_TRUE(ok) << err;
  EXPECT_EQ(detail_name("unit_bl_method", id), "renamed");
}

// ---------------------------------------------------------------------------
// update_block_size
// ---------------------------------------------------------------------------

TEST_F(BlockServiceTest, UpdateBlockSizePersists)
{
  make_schema();
  auto [id, msg] = create_block(conn(), schema, "u1", BlockType::Method, 0, 0, 50, 20, "m");
  ASSERT_FALSE(id.empty()) << msg;

  //
  //
  auto [ok, err] = update_block_size(conn(), schema, id, 222, 33);
  //
  //

  ASSERT_TRUE(ok) << err;
  auto [blocks, lerr] = load_blocks_in_view(conn(), schema, "u1", -1000, -1000, 1000, 1000);
  ASSERT_TRUE(lerr.empty()) << lerr;
  ASSERT_EQ(blocks.size(), 1u);
  EXPECT_FLOAT_EQ(blocks[0].width, 222);
  EXPECT_FLOAT_EQ(blocks[0].height, 33);
}

// ---------------------------------------------------------------------------
// update_block_position
// ---------------------------------------------------------------------------

TEST_F(BlockServiceTest, UpdateBlockPositionPersists)
{
  make_schema();
  auto [id, msg] = create_block(conn(), schema, "u1", BlockType::Method, 0, 0, 50, 20, "m");
  ASSERT_FALSE(id.empty()) << msg;

  //
  //
  auto [ok, err] = update_block_position(conn(), schema, id, 123, 456);
  //
  //

  ASSERT_TRUE(ok) << err;
  auto [blocks, lerr] = load_blocks_in_view(conn(), schema, "u1", 100, 400, 200, 500);
  ASSERT_TRUE(lerr.empty()) << lerr;
  ASSERT_EQ(blocks.size(), 1u);
  EXPECT_FLOAT_EQ(blocks[0].x, 123);
  EXPECT_FLOAT_EQ(blocks[0].y, 456);
}
