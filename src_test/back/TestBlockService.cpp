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
  // Name stored in the detail table for a given unit_b id ("" if absent).
  std::string detail_name(const std::string &table, const std::string &id)
  {
    pqxx::work txn(*pg);
    auto       r = txn.exec_params("SELECT name FROM " + qual(table) + " WHERE id = $1", id);
    return r.empty() ? "" : r[0][0].c_str();
  }

  long unit_b_count()
  {
    pqxx::work txn(*pg);
    return txn.exec("SELECT count(*) FROM " + qual("unit_b"))[0][0].as<long>();
  }
};

// ---------------------------------------------------------------------------
// create_block
// ---------------------------------------------------------------------------

TEST_F(BlockServiceTest, CreateBlockCreatesTablesIfMissing)
{
  make_schema();
  ASSERT_FALSE(table_exists("unit_b"));

  //
  //
  auto [id, msg] = create_block(conn(), schema, "u1", BlockType::Method, 0, 0, 100, 40, "m");
  //
  //

  ASSERT_FALSE(id.empty()) << msg;
  EXPECT_TRUE(table_exists("unit_b"));
  EXPECT_TRUE(table_exists("unit_b_method"));
  EXPECT_TRUE(table_exists("unit_b_field"));
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
  EXPECT_EQ(unit_b_count(), 1);
  EXPECT_EQ(detail_name("unit_b_method", id), "doWork");
  EXPECT_EQ(detail_name("unit_b_field", id), ""); // not in the field table
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
  EXPECT_EQ(detail_name("unit_b_field", id), "count");
  EXPECT_EQ(detail_name("unit_b_method", id), "");
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
  make_schema(); // schema exists, but no unit_b table

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
  EXPECT_EQ(detail_name("unit_b_method", id), "renamed");
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

// ---------------------------------------------------------------------------
// create_method_arg / update_method_arg_name / arg loading
// ---------------------------------------------------------------------------

TEST_F(BlockServiceTest, CreateMethodArgInsertsRow)
{
  make_schema();
  auto [mid, mmsg] = create_block(conn(), schema, "u1", BlockType::Method, 0, 0, 50, 20, "m");
  ASSERT_FALSE(mid.empty()) << mmsg;

  //
  //
  auto [aid, amsg] = create_method_arg(conn(), schema, mid, 0, "arg0");
  //
  //

  ASSERT_FALSE(aid.empty()) << amsg;
  EXPECT_EQ(detail_name("unit_b_method_arg", aid), "arg0");
}

TEST_F(BlockServiceTest, LoadAttachesArgsToMethodsInOrder)
{
  make_schema();
  auto [mid, mmsg] = create_block(conn(), schema, "u1", BlockType::Method, 0, 0, 50, 20, "m");
  ASSERT_FALSE(mid.empty()) << mmsg;
  // Insert out of order; load must sort by order_index.
  ASSERT_FALSE(create_method_arg(conn(), schema, mid, 1, "second").first.empty());
  ASSERT_FALSE(create_method_arg(conn(), schema, mid, 0, "first").first.empty());

  //
  //
  auto [blocks, err] = load_blocks_in_view(conn(), schema, "u1", -1000, -1000, 1000, 1000);
  //
  //

  ASSERT_TRUE(err.empty()) << err;
  ASSERT_EQ(blocks.size(), 1u);
  ASSERT_EQ(blocks[0].args.size(), 2u);
  EXPECT_EQ(blocks[0].args[0].name, "first");
  EXPECT_EQ(blocks[0].args[1].name, "second");
}

TEST_F(BlockServiceTest, UpdateMethodArgNamePersists)
{
  make_schema();
  auto [mid, mmsg] = create_block(conn(), schema, "u1", BlockType::Method, 0, 0, 50, 20, "m");
  ASSERT_FALSE(mid.empty()) << mmsg;
  auto [aid, amsg] = create_method_arg(conn(), schema, mid, 0, "old");
  ASSERT_FALSE(aid.empty()) << amsg;

  //
  //
  auto [ok, err] = update_method_arg_name(conn(), schema, aid, "renamed");
  //
  //

  ASSERT_TRUE(ok) << err;
  EXPECT_EQ(detail_name("unit_b_method_arg", aid), "renamed");
}

TEST_F(BlockServiceTest, DeleteMethodArgRemovesRow)
{
  make_schema();
  auto [mid, mmsg] = create_block(conn(), schema, "u1", BlockType::Method, 0, 0, 50, 20, "m");
  ASSERT_FALSE(mid.empty()) << mmsg;
  auto [aid, amsg] = create_method_arg(conn(), schema, mid, 0, "doomed");
  ASSERT_FALSE(aid.empty()) << amsg;

  //
  //
  auto [ok, err] = delete_method_arg(conn(), schema, aid);
  //
  //

  ASSERT_TRUE(ok) << err;
  EXPECT_EQ(detail_name("unit_b_method_arg", aid), "");
  auto [blocks, lerr] = load_blocks_in_view(conn(), schema, "u1", -1000, -1000, 1000, 1000);
  ASSERT_TRUE(lerr.empty()) << lerr;
  ASSERT_EQ(blocks.size(), 1u);
  EXPECT_TRUE(blocks[0].args.empty());
}

TEST_F(BlockServiceTest, AppendMethodArgGoesToEndAfterDelete)
{
  make_schema();
  auto [mid, mmsg] = create_block(conn(), schema, "u1", BlockType::Method, 0, 0, 50, 20, "m");
  ASSERT_FALSE(mid.empty()) << mmsg;
  auto [a0, m0] = append_method_arg(conn(), schema, mid, "a0");
  ASSERT_FALSE(a0.empty()) << m0;
  auto [a1, m1] = append_method_arg(conn(), schema, mid, "a1");
  ASSERT_FALSE(a1.empty()) << m1;
  // Delete the first arg, then append a new one: it must land at the end.
  ASSERT_TRUE(delete_method_arg(conn(), schema, a0).first);

  //
  //
  auto [a2, m2] = append_method_arg(conn(), schema, mid, "a2");
  //
  //

  ASSERT_FALSE(a2.empty()) << m2;
  auto [blocks, err] = load_blocks_in_view(conn(), schema, "u1", -1000, -1000, 1000, 1000);
  ASSERT_TRUE(err.empty()) << err;
  ASSERT_EQ(blocks.size(), 1u);
  ASSERT_EQ(blocks[0].args.size(), 2u);
  EXPECT_EQ(blocks[0].args[0].name, "a1");
  EXPECT_EQ(blocks[0].args[1].name, "a2");
}

TEST_F(BlockServiceTest, ReorderMethodArgsRewritesOrder)
{
  make_schema();
  auto [mid, mmsg] = create_block(conn(), schema, "u1", BlockType::Method, 0, 0, 50, 20, "m");
  ASSERT_FALSE(mid.empty()) << mmsg;
  auto [a0, m0] = append_method_arg(conn(), schema, mid, "a0");
  auto [a1, m1] = append_method_arg(conn(), schema, mid, "a1");
  auto [a2, m2] = append_method_arg(conn(), schema, mid, "a2");
  ASSERT_FALSE(a0.empty() || a1.empty() || a2.empty());

  //
  //
  auto [ok, err] = reorder_method_args(conn(), schema, mid, {a2, a0, a1});
  //
  //

  ASSERT_TRUE(ok) << err;
  auto [blocks, lerr] = load_blocks_in_view(conn(), schema, "u1", -1000, -1000, 1000, 1000);
  ASSERT_TRUE(lerr.empty()) << lerr;
  ASSERT_EQ(blocks.size(), 1u);
  ASSERT_EQ(blocks[0].args.size(), 3u);
  EXPECT_EQ(blocks[0].args[0].name, "a2");
  EXPECT_EQ(blocks[0].args[1].name, "a0");
  EXPECT_EQ(blocks[0].args[2].name, "a1");
}

// ---------------------------------------------------------------------------
// method attributes: disabled / type / access
// ---------------------------------------------------------------------------

TEST_F(BlockServiceTest, NewMethodHasDefaultAttributes)
{
  make_schema();
  ASSERT_FALSE(create_block(conn(), schema, "u1", BlockType::Method, 0, 0, 50, 20, "m").first.empty());

  //
  //
  auto [blocks, err] = load_blocks_in_view(conn(), schema, "u1", -1000, -1000, 1000, 1000);
  //
  //

  ASSERT_TRUE(err.empty()) << err;
  ASSERT_EQ(blocks.size(), 1u);
  EXPECT_FALSE(blocks[0].disabled);
  EXPECT_EQ(blocks[0].method_type, MethodType::Inner);
  EXPECT_EQ(blocks[0].access, MethodAccess::Private);
}

TEST_F(BlockServiceTest, UpdateMethodAttributesPersist)
{
  make_schema();
  auto [mid, mmsg] = create_block(conn(), schema, "u1", BlockType::Method, 0, 0, 50, 20, "m");
  ASSERT_FALSE(mid.empty()) << mmsg;

  //
  //
  auto disabled = update_block_disabled(conn(), schema, mid, true);
  auto type     = update_method_type(conn(), schema, mid, MethodType::Constructor);
  auto access   = update_method_access(conn(), schema, mid, MethodAccess::Public);
  //
  //

  ASSERT_TRUE(disabled.first) << disabled.second;
  ASSERT_TRUE(type.first) << type.second;
  ASSERT_TRUE(access.first) << access.second;

  auto [blocks, err] = load_blocks_in_view(conn(), schema, "u1", -1000, -1000, 1000, 1000);
  ASSERT_TRUE(err.empty()) << err;
  ASSERT_EQ(blocks.size(), 1u);
  EXPECT_TRUE(blocks[0].disabled);
  EXPECT_EQ(blocks[0].method_type, MethodType::Constructor);
  EXPECT_EQ(blocks[0].access, MethodAccess::Public);
}

TEST_F(BlockServiceTest, NewFieldHasDefaultAttributes)
{
  make_schema();
  ASSERT_FALSE(create_block(conn(), schema, "u1", BlockType::Field, 0, 0, 50, 20, "f").first.empty());

  //
  //
  auto [blocks, err] = load_blocks_in_view(conn(), schema, "u1", -1000, -1000, 1000, 1000);
  //
  //

  ASSERT_TRUE(err.empty()) << err;
  ASSERT_EQ(blocks.size(), 1u);
  EXPECT_EQ(blocks[0].type, BlockType::Field);
  EXPECT_FALSE(blocks[0].disabled);
  EXPECT_EQ(blocks[0].access, MethodAccess::Private);
}

TEST_F(BlockServiceTest, UpdateFieldAttributesPersist)
{
  make_schema();
  auto [fid, fmsg] = create_block(conn(), schema, "u1", BlockType::Field, 0, 0, 50, 20, "f");
  ASSERT_FALSE(fid.empty()) << fmsg;

  //
  //
  auto disabled = update_block_disabled(conn(), schema, fid, true);
  auto access   = update_field_access(conn(), schema, fid, MethodAccess::Protected);
  //
  //

  ASSERT_TRUE(disabled.first) << disabled.second;
  ASSERT_TRUE(access.first) << access.second;

  auto [blocks, err] = load_blocks_in_view(conn(), schema, "u1", -1000, -1000, 1000, 1000);
  ASSERT_TRUE(err.empty()) << err;
  ASSERT_EQ(blocks.size(), 1u);
  EXPECT_TRUE(blocks[0].disabled);
  EXPECT_EQ(blocks[0].access, MethodAccess::Protected);
}

TEST_F(BlockServiceTest, UpdateFieldExprIdUsedPersists)
{
  make_schema();
  auto [fid, fmsg] = create_block(conn(), schema, "u1", BlockType::Field, 0, 0, 50, 20, "f");
  ASSERT_FALSE(fid.empty()) << fmsg;

  auto [before, before_err] = load_blocks_in_view(conn(), schema, "u1", -1000, -1000, 1000, 1000);
  ASSERT_TRUE(before_err.empty()) << before_err;
  ASSERT_EQ(before.size(), 1u);
  ASSERT_FALSE(before[0].expr_id_used);

  //
  //
  auto used = update_field_expr_id_used(conn(), schema, fid, true);
  //
  //

  ASSERT_TRUE(used.first) << used.second;
  auto [blocks, err] = load_blocks_in_view(conn(), schema, "u1", -1000, -1000, 1000, 1000);
  ASSERT_TRUE(err.empty()) << err;
  ASSERT_EQ(blocks.size(), 1u);
  EXPECT_TRUE(blocks[0].expr_id_used);
}

TEST_F(BlockServiceTest, DeleteBlockRemovesMethodAndItsArgs)
{
  make_schema();
  auto [mid, mmsg] = create_block(conn(), schema, "u1", BlockType::Method, 0, 0, 50, 20, "m");
  ASSERT_FALSE(mid.empty()) << mmsg;
  ASSERT_FALSE(create_method_arg(conn(), schema, mid, 0, "a").first.empty());
  // A second block must survive the delete.
  ASSERT_FALSE(create_block(conn(), schema, "u1", BlockType::Field, 0, 30, 50, 20, "f").first.empty());

  //
  //
  auto del = delete_block(conn(), schema, mid, BlockType::Method);
  //
  //

  ASSERT_TRUE(del.first) << del.second;

  auto [blocks, err] = load_blocks_in_view(conn(), schema, "u1", -1000, -1000, 1000, 1000);
  ASSERT_TRUE(err.empty()) << err;
  ASSERT_EQ(blocks.size(), 1u);
  EXPECT_EQ(blocks[0].type, BlockType::Field);
}

TEST_F(BlockServiceTest, DeleteBlockRemovesField)
{
  make_schema();
  auto [fid, fmsg] = create_block(conn(), schema, "u1", BlockType::Field, 0, 0, 50, 20, "f");
  ASSERT_FALSE(fid.empty()) << fmsg;

  //
  //
  auto del = delete_block(conn(), schema, fid, BlockType::Field);
  //
  //

  ASSERT_TRUE(del.first) << del.second;

  auto [blocks, err] = load_blocks_in_view(conn(), schema, "u1", -1000, -1000, 1000, 1000);
  ASSERT_TRUE(err.empty()) << err;
  EXPECT_TRUE(blocks.empty());
}
