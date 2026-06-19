#include "back/DbTestBase.h"
#include "back/service/BlockServiceR.h"
#include "back/service/BlockServiceRW.h"
#include <gtest/gtest.h>

#include <string>
#include <vector>

class BlockServiceRWTest : public DbTest
{
protected:
  // Name stored in the detail table for a given unit_b id ("" if absent).
  std::string detail_name(const std::string &table, const std::string &id)
  {
    pqxx::work txn(*pg);
    auto       r = txn.exec("SELECT name FROM " + qual(table) + " WHERE id = $1", pqxx::params{txn, id});
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

TEST_F(BlockServiceRWTest, CreateBlockCreatesTablesIfMissing)
{
  make_schema();
  ASSERT_FALSE(table_exists("unit_b"));

  //
  //
  auto [id, msg] = back::create_block(conn(), schema, "u1", back::model::BlockType::Method, 0, 0, 100, 40, "m");
  //
  //

  ASSERT_FALSE(id.empty()) << msg;
  EXPECT_TRUE(table_exists("unit_b"));
  EXPECT_TRUE(table_exists("unit_b_method"));
  EXPECT_TRUE(table_exists("unit_b_field"));
}

TEST_F(BlockServiceRWTest, CreateMethodInsertsUnitBlAndMethodRow)
{
  make_schema();

  //
  //
  auto [id, msg] = back::create_block(conn(), schema, "u1", back::model::BlockType::Method, 10, 20, 30, 40, "doWork");
  //
  //

  ASSERT_FALSE(id.empty()) << msg;
  EXPECT_EQ(unit_b_count(), 1);
  EXPECT_EQ(detail_name("unit_b_method", id), "doWork");
  EXPECT_EQ(detail_name("unit_b_field", id), ""); // not in the field table
}

TEST_F(BlockServiceRWTest, CreateFieldInsertsIntoFieldTable)
{
  make_schema();

  //
  //
  auto [id, msg] = back::create_block(conn(), schema, "u1", back::model::BlockType::Field, 0, 0, 100, 40, "count");
  //
  //

  ASSERT_FALSE(id.empty()) << msg;
  EXPECT_EQ(detail_name("unit_b_field", id), "count");
  EXPECT_EQ(detail_name("unit_b_method", id), "");
}

// ---------------------------------------------------------------------------
// update_block_name
// ---------------------------------------------------------------------------

TEST_F(BlockServiceRWTest, UpdateBlockNamePersists)
{
  make_schema();
  auto [id, msg] = back::create_block(conn(), schema, "u1", back::model::BlockType::Method, 0, 0, 50, 20, "old");
  ASSERT_FALSE(id.empty()) << msg;

  //
  //
  auto [ok, err] = back::update_block_name(conn(), schema, id, back::model::BlockType::Method, "renamed");
  //
  //

  ASSERT_TRUE(ok) << err;
  EXPECT_EQ(detail_name("unit_b_method", id), "renamed");
}

// ---------------------------------------------------------------------------
// update_block_size
// ---------------------------------------------------------------------------

TEST_F(BlockServiceRWTest, UpdateBlockSizePersists)
{
  make_schema();
  auto [id, msg] = back::create_block(conn(), schema, "u1", back::model::BlockType::Method, 0, 0, 50, 20, "m");
  ASSERT_FALSE(id.empty()) << msg;

  //
  //
  auto [ok, err] = back::update_block_size(conn(), schema, id, 222, 33);
  //
  //

  ASSERT_TRUE(ok) << err;
  auto [blocks, lerr] = back::load_blocks_in_view(conn(), schema, "u1", -1000, -1000, 1000, 1000);
  ASSERT_TRUE(lerr.empty()) << lerr;
  ASSERT_EQ(blocks.size(), 1u);
  EXPECT_FLOAT_EQ(blocks[0].width, 222);
  EXPECT_FLOAT_EQ(blocks[0].height, 33);
}

// ---------------------------------------------------------------------------
// update_block_position
// ---------------------------------------------------------------------------

TEST_F(BlockServiceRWTest, UpdateBlockPositionPersists)
{
  make_schema();
  auto [id, msg] = back::create_block(conn(), schema, "u1", back::model::BlockType::Method, 0, 0, 50, 20, "m");
  ASSERT_FALSE(id.empty()) << msg;

  //
  //
  auto [ok, err] = back::update_block_position(conn(), schema, id, 123, 456);
  //
  //

  ASSERT_TRUE(ok) << err;
  auto [blocks, lerr] = back::load_blocks_in_view(conn(), schema, "u1", 100, 400, 200, 500);
  ASSERT_TRUE(lerr.empty()) << lerr;
  ASSERT_EQ(blocks.size(), 1u);
  EXPECT_FLOAT_EQ(blocks[0].x, 123);
  EXPECT_FLOAT_EQ(blocks[0].y, 456);
}

// ---------------------------------------------------------------------------
// create_method_arg / update_method_arg_name / arg loading
// ---------------------------------------------------------------------------

TEST_F(BlockServiceRWTest, CreateMethodArgInsertsRow)
{
  make_schema();
  auto [mid, mmsg] = back::create_block(conn(), schema, "u1", back::model::BlockType::Method, 0, 0, 50, 20, "m");
  ASSERT_FALSE(mid.empty()) << mmsg;

  //
  //
  auto [aid, amsg] = back::create_method_arg(conn(), schema, mid, 0, "arg0");
  //
  //

  ASSERT_FALSE(aid.empty()) << amsg;
  EXPECT_EQ(detail_name("unit_b_method_arg", aid), "arg0");
}

TEST_F(BlockServiceRWTest, UpdateMethodArgNamePersists)
{
  make_schema();
  auto [mid, mmsg] = back::create_block(conn(), schema, "u1", back::model::BlockType::Method, 0, 0, 50, 20, "m");
  ASSERT_FALSE(mid.empty()) << mmsg;
  auto [aid, amsg] = back::create_method_arg(conn(), schema, mid, 0, "old");
  ASSERT_FALSE(aid.empty()) << amsg;

  //
  //
  auto [ok, err] = back::update_method_arg_name(conn(), schema, aid, "renamed");
  //
  //

  ASSERT_TRUE(ok) << err;
  EXPECT_EQ(detail_name("unit_b_method_arg", aid), "renamed");
}

TEST_F(BlockServiceRWTest, DeleteMethodArgRemovesRow)
{
  make_schema();
  auto [mid, mmsg] = back::create_block(conn(), schema, "u1", back::model::BlockType::Method, 0, 0, 50, 20, "m");
  ASSERT_FALSE(mid.empty()) << mmsg;
  auto [aid, amsg] = back::create_method_arg(conn(), schema, mid, 0, "doomed");
  ASSERT_FALSE(aid.empty()) << amsg;

  //
  //
  auto [ok, err] = back::delete_method_arg(conn(), schema, aid);
  //
  //

  ASSERT_TRUE(ok) << err;
  EXPECT_EQ(detail_name("unit_b_method_arg", aid), "");
  auto [blocks, lerr] = back::load_blocks_in_view(conn(), schema, "u1", -1000, -1000, 1000, 1000);
  ASSERT_TRUE(lerr.empty()) << lerr;
  ASSERT_EQ(blocks.size(), 1u);
  EXPECT_TRUE(blocks[0].args.empty());
}

TEST_F(BlockServiceRWTest, AppendMethodArgGoesToEndAfterDelete)
{
  make_schema();
  auto [mid, mmsg] = back::create_block(conn(), schema, "u1", back::model::BlockType::Method, 0, 0, 50, 20, "m");
  ASSERT_FALSE(mid.empty()) << mmsg;
  auto [a0, m0] = back::append_method_arg(conn(), schema, mid, "a0");
  ASSERT_FALSE(a0.empty()) << m0;
  auto [a1, m1] = back::append_method_arg(conn(), schema, mid, "a1");
  ASSERT_FALSE(a1.empty()) << m1;
  // Delete the first arg, then append a new one: it must land at the end.
  ASSERT_TRUE(back::delete_method_arg(conn(), schema, a0).first);

  //
  //
  auto [a2, m2] = back::append_method_arg(conn(), schema, mid, "a2");
  //
  //

  ASSERT_FALSE(a2.empty()) << m2;
  auto [blocks, err] = back::load_blocks_in_view(conn(), schema, "u1", -1000, -1000, 1000, 1000);
  ASSERT_TRUE(err.empty()) << err;
  ASSERT_EQ(blocks.size(), 1u);
  ASSERT_EQ(blocks[0].args.size(), 2u);
  EXPECT_EQ(blocks[0].args[0].name, "a1");
  EXPECT_EQ(blocks[0].args[1].name, "a2");
}

TEST_F(BlockServiceRWTest, ReorderMethodArgsRewritesOrder)
{
  make_schema();
  auto [mid, mmsg] = back::create_block(conn(), schema, "u1", back::model::BlockType::Method, 0, 0, 50, 20, "m");
  ASSERT_FALSE(mid.empty()) << mmsg;
  auto [a0, m0] = back::append_method_arg(conn(), schema, mid, "a0");
  auto [a1, m1] = back::append_method_arg(conn(), schema, mid, "a1");
  auto [a2, m2] = back::append_method_arg(conn(), schema, mid, "a2");
  ASSERT_FALSE(a0.empty() || a1.empty() || a2.empty());

  //
  //
  auto [ok, err] = back::reorder_method_args(conn(), schema, mid, {a2, a0, a1});
  //
  //

  ASSERT_TRUE(ok) << err;
  auto [blocks, lerr] = back::load_blocks_in_view(conn(), schema, "u1", -1000, -1000, 1000, 1000);
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

TEST_F(BlockServiceRWTest, UpdateMethodAttributesPersist)
{
  make_schema();
  auto [mid, mmsg] = back::create_block(conn(), schema, "u1", back::model::BlockType::Method, 0, 0, 50, 20, "m");
  ASSERT_FALSE(mid.empty()) << mmsg;

  //
  //
  auto disabled = back::update_block_disabled(conn(), schema, mid, true);
  auto type     = back::update_method_type(conn(), schema, mid, back::model::MethodType::Constructor);
  auto access   = back::update_method_access(conn(), schema, mid, back::model::MethodAccess::Public);
  //
  //

  ASSERT_TRUE(disabled.first) << disabled.second;
  ASSERT_TRUE(type.first) << type.second;
  ASSERT_TRUE(access.first) << access.second;

  auto [blocks, err] = back::load_blocks_in_view(conn(), schema, "u1", -1000, -1000, 1000, 1000);
  ASSERT_TRUE(err.empty()) << err;
  ASSERT_EQ(blocks.size(), 1u);
  EXPECT_TRUE(blocks[0].disabled);
  EXPECT_EQ(blocks[0].method_type, back::model::MethodType::Constructor);
  EXPECT_EQ(blocks[0].access, back::model::MethodAccess::Public);
}

TEST_F(BlockServiceRWTest, UpdateFieldAttributesPersist)
{
  make_schema();
  auto [fid, fmsg] = back::create_block(conn(), schema, "u1", back::model::BlockType::Field, 0, 0, 50, 20, "f");
  ASSERT_FALSE(fid.empty()) << fmsg;

  //
  //
  auto disabled = back::update_block_disabled(conn(), schema, fid, true);
  auto access   = back::update_field_access(conn(), schema, fid, back::model::MethodAccess::Protected);
  //
  //

  ASSERT_TRUE(disabled.first) << disabled.second;
  ASSERT_TRUE(access.first) << access.second;

  auto [blocks, err] = back::load_blocks_in_view(conn(), schema, "u1", -1000, -1000, 1000, 1000);
  ASSERT_TRUE(err.empty()) << err;
  ASSERT_EQ(blocks.size(), 1u);
  EXPECT_TRUE(blocks[0].disabled);
  EXPECT_EQ(blocks[0].access, back::model::MethodAccess::Protected);
}

TEST_F(BlockServiceRWTest, UpdateFieldExprIdUsedPersists)
{
  make_schema();
  auto [fid, fmsg] = back::create_block(conn(), schema, "u1", back::model::BlockType::Field, 0, 0, 50, 20, "f");
  ASSERT_FALSE(fid.empty()) << fmsg;

  auto [before, before_err] = back::load_blocks_in_view(conn(), schema, "u1", -1000, -1000, 1000, 1000);
  ASSERT_TRUE(before_err.empty()) << before_err;
  ASSERT_EQ(before.size(), 1u);
  ASSERT_FALSE(before[0].expr_id_used);

  //
  //
  auto used = back::update_field_expr_id_used(conn(), schema, fid, true);
  //
  //

  ASSERT_TRUE(used.first) << used.second;
  auto [blocks, err] = back::load_blocks_in_view(conn(), schema, "u1", -1000, -1000, 1000, 1000);
  ASSERT_TRUE(err.empty()) << err;
  ASSERT_EQ(blocks.size(), 1u);
  EXPECT_TRUE(blocks[0].expr_id_used);
}

TEST_F(BlockServiceRWTest, UpdateFieldSizeBytesPersists)
{
  make_schema();
  auto [fid, fmsg] = back::create_block(conn(), schema, "u1", back::model::BlockType::Field, 0, 0, 50, 20, "f");
  ASSERT_FALSE(fid.empty()) << fmsg;

  auto [before, before_err] = back::load_blocks_in_view(conn(), schema, "u1", -1000, -1000, 1000, 1000);
  ASSERT_TRUE(before_err.empty()) << before_err;
  ASSERT_EQ(before.size(), 1u);
  ASSERT_EQ(before[0].size_bytes, 0);

  //
  //
  auto sized = back::update_field_size_bytes(conn(), schema, fid, 34);
  //
  //

  ASSERT_TRUE(sized.first) << sized.second;
  auto [blocks, err] = back::load_blocks_in_view(conn(), schema, "u1", -1000, -1000, 1000, 1000);
  ASSERT_TRUE(err.empty()) << err;
  ASSERT_EQ(blocks.size(), 1u);
  EXPECT_EQ(blocks[0].size_bytes, 34);
}

TEST_F(BlockServiceRWTest, DeleteBlockRemovesMethodAndItsArgs)
{
  make_schema();
  auto [mid, mmsg] = back::create_block(conn(), schema, "u1", back::model::BlockType::Method, 0, 0, 50, 20, "m");
  ASSERT_FALSE(mid.empty()) << mmsg;
  ASSERT_FALSE(back::create_method_arg(conn(), schema, mid, 0, "a").first.empty());
  // A second block must survive the delete.
  ASSERT_FALSE(back::create_block(conn(), schema, "u1", back::model::BlockType::Field, 0, 30, 50, 20, "f").first.empty());

  //
  //
  auto del = back::delete_block(conn(), schema, mid, back::model::BlockType::Method);
  //
  //

  ASSERT_TRUE(del.first) << del.second;

  auto [blocks, err] = back::load_blocks_in_view(conn(), schema, "u1", -1000, -1000, 1000, 1000);
  ASSERT_TRUE(err.empty()) << err;
  ASSERT_EQ(blocks.size(), 1u);
  EXPECT_EQ(blocks[0].type, back::model::BlockType::Field);
}

TEST_F(BlockServiceRWTest, DeleteBlockRemovesField)
{
  make_schema();
  auto [fid, fmsg] = back::create_block(conn(), schema, "u1", back::model::BlockType::Field, 0, 0, 50, 20, "f");
  ASSERT_FALSE(fid.empty()) << fmsg;

  //
  //
  auto del = back::delete_block(conn(), schema, fid, back::model::BlockType::Field);
  //
  //

  ASSERT_TRUE(del.first) << del.second;

  auto [blocks, err] = back::load_blocks_in_view(conn(), schema, "u1", -1000, -1000, 1000, 1000);
  ASSERT_TRUE(err.empty()) << err;
  EXPECT_TRUE(blocks.empty());
}
