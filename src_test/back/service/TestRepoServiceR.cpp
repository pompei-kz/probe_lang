#include "back/DbTestBase.h"
#include "back/service/FolderServiceRW.h" // create_folder, for the folders-loading test
#include "back/service/RepoServiceR.h"
#include "back/service/RepoServiceRW.h" // create_repository, for connect_and_load arrange
#include <gtest/gtest.h>

#include <string>
#include <vector>

class RepoServiceRTest : public DbTest
{
protected:
  std::string repo_name_in(const std::string &sch)
  {
    pqxx::work txn(*pg);
    auto       r = txn.exec("SELECT value FROM " + pg->quote_name(sch) + ".lang_setting WHERE name='name'");
    return r.empty() ? "" : r[0][0].as<std::string>();
  }
};

TEST_F(RepoServiceRTest, TestConnectionSucceedsWithValidCreds)
{
  //
  //
  auto [ok, msg] = back::test_connection(test_db::HOST, test_db::PORT, test_db::DBNAME, test_db::USER, test_db::PASS);
  //
  //

  EXPECT_TRUE(ok) << msg;
}

TEST_F(RepoServiceRTest, TestConnectionFailsWithBadPassword)
{
  //
  //
  auto [ok, msg] = back::test_connection(test_db::HOST, test_db::PORT, test_db::DBNAME, test_db::USER, "definitely-wrong");
  //
  //

  EXPECT_FALSE(ok);
  EXPECT_FALSE(msg.empty());
}

TEST_F(RepoServiceRTest, ConnectAndLoadReturnsCreatedRepo)
{
  ASSERT_TRUE(back::create_repository(conn(), schema, "Visible Repo").first);

  std::vector<back::model::RepoNode> repos;

  //
  //
  auto [ok, msg] = back::connect_and_load(conn(), repos);
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

TEST_F(RepoServiceRTest, ConnectAndLoadIncludesFolders)
{
  ASSERT_TRUE(back::create_repository(conn(), schema, "R").first);
  ASSERT_TRUE(back::create_folder(conn(), schema, "", "Docs").first);

  std::vector<back::model::RepoNode> repos;
  //
  //
  auto [ok, msg] = back::connect_and_load(conn(), repos);
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
