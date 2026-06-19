#include "back/DbTestBase.h"
#include "back/service/BlockServiceR.h"
#include "back/service/BlockServiceRW.h"
#include <gtest/gtest.h>

#include <string>
#include <vector>

class BlockServiceRTest : public DbTest
{
};

// ---------------------------------------------------------------------------
// load_blocks_in_view
// ---------------------------------------------------------------------------

TEST_F(BlockServiceRTest, LoadReturnsBlockIntersectingView)
{
  make_schema();
  auto [id, msg] = back::create_block(conn(), schema, "u1", back::model::BlockType::Method, 10, 20, 30, 40, "m");
  ASSERT_FALSE(id.empty()) << msg;

  //
  //
  auto [blocks, err] = back::load_blocks_in_view(conn(), schema, "u1", 0, 0, 100, 100);
  //
  //

  ASSERT_TRUE(err.empty()) << err;
  ASSERT_EQ(blocks.size(), 1u);
  EXPECT_EQ(blocks[0].id, id);
  EXPECT_EQ(blocks[0].unit_id, "u1");
  EXPECT_EQ(blocks[0].type, back::model::BlockType::Method);
  EXPECT_EQ(blocks[0].name, "m");
  EXPECT_FLOAT_EQ(blocks[0].x, 10);
  EXPECT_FLOAT_EQ(blocks[0].y, 20);
  EXPECT_FLOAT_EQ(blocks[0].width, 30);
  EXPECT_FLOAT_EQ(blocks[0].height, 40);
}

TEST_F(BlockServiceRTest, LoadExcludesBlockOutsideView)
{
  make_schema();
  auto [id, msg] = back::create_block(conn(), schema, "u1", back::model::BlockType::Method, 10, 20, 30, 40, "m");
  ASSERT_FALSE(id.empty()) << msg;

  //
  //
  auto [blocks, err] = back::load_blocks_in_view(conn(), schema, "u1", 500, 500, 600, 600);
  //
  //

  ASSERT_TRUE(err.empty()) << err;
  EXPECT_TRUE(blocks.empty());
}

TEST_F(BlockServiceRTest, LoadExcludesBlocksOfOtherUnits)
{
  make_schema();
  ASSERT_FALSE(back::create_block(conn(), schema, "u1", back::model::BlockType::Method, 0, 0, 50, 20, "mine").first.empty());
  ASSERT_FALSE(back::create_block(conn(), schema, "u2", back::model::BlockType::Method, 0, 30, 50, 20, "other").first.empty());

  //
  //
  auto [blocks, err] = back::load_blocks_in_view(conn(), schema, "u1", -1000, -1000, 1000, 1000);
  //
  //

  ASSERT_TRUE(err.empty()) << err;
  ASSERT_EQ(blocks.size(), 1u);
  EXPECT_EQ(blocks[0].name, "mine");
}

TEST_F(BlockServiceRTest, LoadReturnsBothMethodAndFieldNames)
{
  make_schema();
  ASSERT_FALSE(back::create_block(conn(), schema, "u1", back::model::BlockType::Method, 0, 0, 50, 20, "doWork").first.empty());
  ASSERT_FALSE(back::create_block(conn(), schema, "u1", back::model::BlockType::Field, 0, 30, 50, 20, "count").first.empty());

  //
  //
  auto [blocks, err] = back::load_blocks_in_view(conn(), schema, "u1", -1000, -1000, 1000, 1000);
  //
  //

  ASSERT_TRUE(err.empty()) << err;
  ASSERT_EQ(blocks.size(), 2u);
  bool sawMethod = false, sawField = false;
  for (const auto &s : blocks) {
    if (s.type == back::model::BlockType::Method) {
      sawMethod = (s.name == "doWork");
    }
    if (s.type == back::model::BlockType::Field) {
      sawField = (s.name == "count");
    }
  }
  EXPECT_TRUE(sawMethod);
  EXPECT_TRUE(sawField);
}

TEST_F(BlockServiceRTest, LoadEmptyWhenTableMissing)
{
  make_schema(); // schema exists, but no unit_b table

  //
  //
  auto [blocks, err] = back::load_blocks_in_view(conn(), schema, "u1", 0, 0, 100, 100);
  //
  //

  ASSERT_TRUE(err.empty()) << err;
  EXPECT_TRUE(blocks.empty());
}

TEST_F(BlockServiceRTest, LoadAttachesArgsToMethodsInOrder)
{
  make_schema();
  auto [mid, mmsg] = back::create_block(conn(), schema, "u1", back::model::BlockType::Method, 0, 0, 50, 20, "m");
  ASSERT_FALSE(mid.empty()) << mmsg;
  // Insert out of order; load must sort by order_index.
  ASSERT_FALSE(back::create_method_arg(conn(), schema, mid, 1, "second").first.empty());
  ASSERT_FALSE(back::create_method_arg(conn(), schema, mid, 0, "first").first.empty());

  //
  //
  auto [blocks, err] = back::load_blocks_in_view(conn(), schema, "u1", -1000, -1000, 1000, 1000);
  //
  //

  ASSERT_TRUE(err.empty()) << err;
  ASSERT_EQ(blocks.size(), 1u);
  ASSERT_EQ(blocks[0].args.size(), 2u);
  EXPECT_EQ(blocks[0].args[0].name, "first");
  EXPECT_EQ(blocks[0].args[1].name, "second");
}

// ---------------------------------------------------------------------------
// block_bbox_for_unit
// ---------------------------------------------------------------------------

TEST_F(BlockServiceRTest, BboxSpansAllBlocksOfUnit)
{
  make_schema();
  ASSERT_FALSE(back::create_block(conn(), schema, "u1", back::model::BlockType::Method, 10, 10, 20, 20, "a").first.empty());
  ASSERT_FALSE(back::create_block(conn(), schema, "u1", back::model::BlockType::Field, 50, 60, 20, 20, "b").first.empty());
  // Another unit's block must not affect u1's bbox.
  ASSERT_FALSE(back::create_block(conn(), schema, "u2", back::model::BlockType::Method, 500, 500, 20, 20, "c").first.empty());

  //
  //
  auto [box, err] = back::block_bbox_for_unit(conn(), schema, "u1");
  //
  //

  ASSERT_TRUE(err.empty()) << err;
  ASSERT_TRUE(box.has_value());
  EXPECT_FLOAT_EQ(box->min_x, 10);
  EXPECT_FLOAT_EQ(box->min_y, 10);
  EXPECT_FLOAT_EQ(box->max_x, 70); // 50 + 20
  EXPECT_FLOAT_EQ(box->max_y, 80); // 60 + 20
}

TEST_F(BlockServiceRTest, BboxNullWhenUnitHasNoBlocks)
{
  make_schema();
  ASSERT_FALSE(back::create_block(conn(), schema, "u1", back::model::BlockType::Method, 0, 0, 20, 20, "a").first.empty());

  //
  //
  auto [box, err] = back::block_bbox_for_unit(conn(), schema, "other");
  //
  //

  ASSERT_TRUE(err.empty()) << err;
  EXPECT_FALSE(box.has_value());
}

// ---------------------------------------------------------------------------
// default attributes (verified through load_blocks_in_view)
// ---------------------------------------------------------------------------

TEST_F(BlockServiceRTest, NewMethodHasDefaultAttributes)
{
  make_schema();
  ASSERT_FALSE(back::create_block(conn(), schema, "u1", back::model::BlockType::Method, 0, 0, 50, 20, "m").first.empty());

  //
  //
  auto [blocks, err] = back::load_blocks_in_view(conn(), schema, "u1", -1000, -1000, 1000, 1000);
  //
  //

  ASSERT_TRUE(err.empty()) << err;
  ASSERT_EQ(blocks.size(), 1u);
  EXPECT_FALSE(blocks[0].disabled);
  EXPECT_EQ(blocks[0].method_type, back::model::MethodType::Inner);
  EXPECT_EQ(blocks[0].access, back::model::MethodAccess::Private);
}

TEST_F(BlockServiceRTest, NewFieldHasDefaultAttributes)
{
  make_schema();
  ASSERT_FALSE(back::create_block(conn(), schema, "u1", back::model::BlockType::Field, 0, 0, 50, 20, "f").first.empty());

  //
  //
  auto [blocks, err] = back::load_blocks_in_view(conn(), schema, "u1", -1000, -1000, 1000, 1000);
  //
  //

  ASSERT_TRUE(err.empty()) << err;
  ASSERT_EQ(blocks.size(), 1u);
  EXPECT_EQ(blocks[0].type, back::model::BlockType::Field);
  EXPECT_FALSE(blocks[0].disabled);
  EXPECT_EQ(blocks[0].access, back::model::MethodAccess::Private);
}
