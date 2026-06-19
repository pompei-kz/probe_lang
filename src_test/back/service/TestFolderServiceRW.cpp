#include "back/DbTestBase.h"
#include "back/service/FolderServiceR.h"
#include "back/service/FolderServiceRW.h"
#include <gtest/gtest.h>

#include <string>
#include <vector>

using namespace back;

class FolderServiceRWTest : public DbTest
{
protected:
  // id of the first folder with the given name ("" if none).
  std::string folder_id(const std::string &name)
  {
    pqxx::work txn(*pg);
    auto       r = txn.exec("SELECT id FROM " + qual("folder") + " WHERE name=$1", pqxx::params{txn, name});
    return r.empty() ? "" : r[0][0].as<std::string>();
  }

  long folder_count()
  {
    pqxx::work txn(*pg);
    return txn.exec("SELECT count(*) FROM " + qual("folder"))[0][0].as<long>();
  }
};

// ---------------------------------------------------------------------------
// create_folder
// ---------------------------------------------------------------------------

TEST_F(FolderServiceRWTest, CreateFolderCreatesTableIfMissing)
{
  make_schema();
  ASSERT_FALSE(table_exists("folder"));

  //
  //
  auto [ok, msg] = create_folder(conn(), schema, "", "X");
  //
  //

  ASSERT_TRUE(ok) << msg;
  EXPECT_TRUE(table_exists("folder"));
}

TEST_F(FolderServiceRWTest, CreateRootFolder)
{
  make_schema();

  //
  //
  auto [ok, msg] = create_folder(conn(), schema, "", "Root");
  //
  //

  ASSERT_TRUE(ok) << msg;
  std::vector<model::FolderNode> roots;
  ASSERT_TRUE(load_repo_folders(conn(), schema, roots).first);
  ASSERT_EQ(roots.size(), 1u);
  EXPECT_EQ(roots[0].name, "Root");
  EXPECT_TRUE(roots[0].parent_id.empty());
}

TEST_F(FolderServiceRWTest, CreateChildFolderBuildsTree)
{
  make_schema();
  ASSERT_TRUE(create_folder(conn(), schema, "", "Parent").first);
  const std::string pid = folder_id("Parent");
  ASSERT_FALSE(pid.empty());

  //
  //
  auto [ok, msg] = create_folder(conn(), schema, pid, "Child");
  //
  //

  ASSERT_TRUE(ok) << msg;
  std::vector<model::FolderNode> roots;
  ASSERT_TRUE(load_repo_folders(conn(), schema, roots).first);
  ASSERT_EQ(roots.size(), 1u);
  EXPECT_EQ(roots[0].name, "Parent");
  ASSERT_EQ(roots[0].children.size(), 1u);
  EXPECT_EQ(roots[0].children[0].name, "Child");
  EXPECT_EQ(roots[0].children[0].parent_id, pid);
}

// ---------------------------------------------------------------------------
// rename_folder
// ---------------------------------------------------------------------------

TEST_F(FolderServiceRWTest, RenameFolder)
{
  make_schema();
  ASSERT_TRUE(create_folder(conn(), schema, "", "Before").first);
  const std::string id = folder_id("Before");

  //
  //
  auto [ok, msg] = rename_folder(conn(), schema, id, "After");
  //
  //

  ASSERT_TRUE(ok) << msg;
  EXPECT_EQ(folder_id("After"), id);
  EXPECT_TRUE(folder_id("Before").empty());
}

// ---------------------------------------------------------------------------
// delete_folder_recursive
// ---------------------------------------------------------------------------

TEST_F(FolderServiceRWTest, DeleteFolderRecursiveRemovesDescendants)
{
  make_schema();
  ASSERT_TRUE(create_folder(conn(), schema, "", "Root").first);
  const std::string root = folder_id("Root");
  ASSERT_TRUE(create_folder(conn(), schema, root, "Child").first);
  const std::string child = folder_id("Child");
  ASSERT_TRUE(create_folder(conn(), schema, child, "Grand").first);
  ASSERT_EQ(folder_count(), 3);

  //
  //
  auto [ok, msg] = delete_folder_recursive(conn(), schema, root);
  //
  //

  ASSERT_TRUE(ok) << msg;
  EXPECT_EQ(folder_count(), 0);
}

TEST_F(FolderServiceRWTest, DeleteFolderRecursiveKeepsSiblings)
{
  make_schema();
  ASSERT_TRUE(create_folder(conn(), schema, "", "A").first);
  ASSERT_TRUE(create_folder(conn(), schema, "", "B").first);
  const std::string a = folder_id("A");

  //
  //
  auto [ok, msg] = delete_folder_recursive(conn(), schema, a);
  //
  //

  ASSERT_TRUE(ok) << msg;
  EXPECT_TRUE(folder_id("A").empty());
  EXPECT_FALSE(folder_id("B").empty());
}
