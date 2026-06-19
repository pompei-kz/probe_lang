#include "back/DbTestBase.h"
#include "back/service/RepoServiceRW.h"
#include <gtest/gtest.h>

#include <string>
#include <vector>

using namespace back;

// ---------------------------------------------------------------------------
// DB-backed tests
// ---------------------------------------------------------------------------

class RepoServiceRWTest : public DbTest
{
protected:
  std::string repo_name_in(const std::string &sch)
  {
    pqxx::work txn(*pg);
    auto       r = txn.exec("SELECT value FROM " + pg->quote_name(sch) + ".lang_setting WHERE name='name'");
    return r.empty() ? "" : r[0][0].as<std::string>();
  }
};

TEST_F(RepoServiceRWTest, CreateRepositoryCreatesSchemaAndTables)
{
  //
  //
  auto [ok, msg] = create_repository(conn(), schema, "My Repo");
  //
  //

  ASSERT_TRUE(ok) << msg;
  EXPECT_TRUE(schema_exists(schema));
  EXPECT_TRUE(table_exists("lang_setting"));
  EXPECT_TRUE(table_exists("folder"));
  EXPECT_EQ(repo_name_in(schema), "My Repo");
}

TEST_F(RepoServiceRWTest, CreateRepositoryAddsAuditColumns)
{
  //
  //
  auto [ok, msg] = create_repository(conn(), schema, "R");
  //
  //

  ASSERT_TRUE(ok) << msg;
  EXPECT_TRUE(column_exists("lang_setting", "created_at"));
  EXPECT_TRUE(column_exists("lang_setting", "last_modified_at"));
}

TEST_F(RepoServiceRWTest, CreateRepositoryIsIdempotentAndUpdatesName)
{
  ASSERT_TRUE(create_repository(conn(), schema, "First").first);

  //
  //
  auto [ok, msg] = create_repository(conn(), schema, "Second"); // ON CONFLICT updates value
  //
  //

  ASSERT_TRUE(ok) << msg;
  EXPECT_EQ(repo_name_in(schema), "Second");
}

TEST_F(RepoServiceRWTest, EditRepositoryRenamesValueOnly)
{
  ASSERT_TRUE(create_repository(conn(), schema, "Old Name").first);

  //
  //
  auto [ok, msg] = edit_repository(conn(), schema, schema, "New Name");
  //
  //

  ASSERT_TRUE(ok) << msg;
  EXPECT_EQ(repo_name_in(schema), "New Name");
}

TEST_F(RepoServiceRWTest, EditRepositoryRenamesSchema)
{
  ASSERT_TRUE(create_repository(conn(), schema, "R").first);
  const std::string new_schema = schema + "_renamed";

  //
  //
  auto [ok, msg] = edit_repository(conn(), schema, new_schema, "R2");
  //
  //

  ASSERT_TRUE(ok) << msg;
  EXPECT_FALSE(schema_exists(schema));
  EXPECT_TRUE(schema_exists(new_schema));
  EXPECT_EQ(repo_name_in(new_schema), "R2");
}
