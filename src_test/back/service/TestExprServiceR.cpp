#include "back/DbTestBase.h"
#include "back/service/BlockServiceR.h"
#include "back/service/BlockServiceRW.h"
#include <gtest/gtest.h>

using namespace back;
using namespace back::model;

class ExprServiceRTest : public DbTest
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

TEST_F(ExprServiceRTest, NoExpressionWhenNoneSet)
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
