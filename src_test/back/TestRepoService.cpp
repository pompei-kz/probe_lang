#include "DbTestBase.h"
#include "back/FolderService.h" // create_folder, for the folders-loading test
#include "back/RepoService.h"
#include <gtest/gtest.h>

#include <string>
#include <vector>

using namespace back;

// ---------------------------------------------------------------------------
// make_cs — pure, no DB needed
// ---------------------------------------------------------------------------

TEST(RepoServiceMakeCs, IncludesProvidedFields)
{
  model::Conn c;
  c.host   = "myhost";
  c.port   = "1234";
  c.dbname = "mydb";
  c.user   = "me";
  c.pass   = "pw";

  //
  //
  const std::string cs = make_cs(c);
  //
  //

  EXPECT_NE(cs.find("host=myhost"), std::string::npos);
  EXPECT_NE(cs.find("port=1234"), std::string::npos);
  EXPECT_NE(cs.find("dbname=mydb"), std::string::npos);
  EXPECT_NE(cs.find("user=me"), std::string::npos);
  EXPECT_NE(cs.find("password=pw"), std::string::npos);
}

TEST(RepoServiceMakeCs, AppliesDefaultsAndOmitsEmptyCredentials)
{
  model::Conn c;
  c.host = "h"; // port, dbname, user, pass left empty

  //
  //
  const std::string cs = make_cs(c);
  //
  //

  EXPECT_NE(cs.find("port=5432"), std::string::npos);
  EXPECT_NE(cs.find("dbname=postgres"), std::string::npos);
  EXPECT_EQ(cs.find("user="), std::string::npos);     // empty user omitted
  EXPECT_EQ(cs.find("password="), std::string::npos); // empty pass omitted
}

// ---------------------------------------------------------------------------
// DB-backed tests
// ---------------------------------------------------------------------------

class RepoServiceTest : public DbTest
{
protected:
  std::string repo_name_in(const std::string &sch)
  {
    pqxx::work txn(*pg);
    auto       r = txn.exec("SELECT value FROM " + pg->quote_name(sch) + ".lang_setting WHERE name='name'");
    return r.empty() ? "" : r[0][0].as<std::string>();
  }
};

TEST_F(RepoServiceTest, TestConnectionSucceedsWithValidCreds)
{
  //
  //
  auto [ok, msg] = test_connection(test_db::HOST, test_db::PORT, test_db::DBNAME, test_db::USER, test_db::PASS);
  //
  //

  EXPECT_TRUE(ok) << msg;
}

TEST_F(RepoServiceTest, TestConnectionFailsWithBadPassword)
{
  //
  //
  auto [ok, msg] = test_connection(test_db::HOST, test_db::PORT, test_db::DBNAME, test_db::USER, "definitely-wrong");
  //
  //

  EXPECT_FALSE(ok);
  EXPECT_FALSE(msg.empty());
}

TEST_F(RepoServiceTest, CreateRepositoryCreatesSchemaAndTables)
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

TEST_F(RepoServiceTest, CreateRepositoryAddsAuditColumns)
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

TEST_F(RepoServiceTest, CreateRepositoryIsIdempotentAndUpdatesName)
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

TEST_F(RepoServiceTest, EditRepositoryRenamesValueOnly)
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

TEST_F(RepoServiceTest, EditRepositoryRenamesSchema)
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

TEST_F(RepoServiceTest, ConnectAndLoadReturnsCreatedRepo)
{
  ASSERT_TRUE(create_repository(conn(), schema, "Visible Repo").first);

  std::vector<model::RepoNode> repos;
  //
  //
  auto [ok, msg] = connect_and_load(conn(), repos);
  //
  //

  ASSERT_TRUE(ok) << msg;
  bool found = false;
  for (const auto &r : repos)
    if (r.schema_name == schema) {
      found = true;
      EXPECT_EQ(r.repo_name, "Visible Repo");
    }
  EXPECT_TRUE(found) << "created schema not present in connect_and_load result";
}

TEST_F(RepoServiceTest, ConnectAndLoadIncludesFolders)
{
  ASSERT_TRUE(create_repository(conn(), schema, "R").first);
  ASSERT_TRUE(create_folder(conn(), schema, "", "Docs").first);

  std::vector<model::RepoNode> repos;
  //
  //
  auto [ok, msg] = connect_and_load(conn(), repos);
  //
  //

  ASSERT_TRUE(ok) << msg;
  bool found = false;
  for (const auto &r : repos)
    if (r.schema_name == schema) {
      found = true;
      ASSERT_EQ(r.folders.size(), 1u);
      EXPECT_EQ(r.folders[0].name, "Docs");
    }
  EXPECT_TRUE(found);
}
